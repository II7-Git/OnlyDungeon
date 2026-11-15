// OnlyDungeon 어빌리티 시스템 컴포넌트

#include "ODAbilitySystemComponent.h"

#include "AbilitySystem/ODGameplayAbility.h"
#include "AbilitySystem/Abilities/ODGameplayAbility_Dodge.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

UODAbilitySystemComponent::UODAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 틱 대신 입력 이벤트로 동작한다. (ASC 자체 틱은 GameplayEffect 기간 처리 등에 필요하므로 엔진 기본값 유지)
}

void UODAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// 캔슬 구간: 지금 행동을 끊고 회피를 발동할 수 있다. (캔슬은 회피만 가능)
	const bool bInCancelWindow = HasMatchingGameplayTag(ODGameplayTags::State_Cancelable);

	// 순회 중에는 어빌리티 목록이 바뀌면 안 되므로, 발동할 핸들을 모아 두었다가 순회가 끝난 뒤 발동한다.
	TArray<FGameplayAbilitySpecHandle> HandlesToActivate;
	TArray<FGameplayAbilitySpecHandle> HandlesToRestart;
	bool bSendBranchInput = false;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
			}

			Spec.InputPressed = true;

			const UODGameplayAbility* ODAbility = Cast<UODGameplayAbility>(Spec.Ability);
			const bool bCanCancel = bInCancelWindow && Spec.Ability->IsA<UODGameplayAbility_Dodge>();

			if (Spec.IsActive())
			{
				// 캔슬 구간에서 회피를 다시 누르면 끊고 새로 발동한다.
				if (bCanCancel && ODAbility && !ODAbility->HandlesInputWhileActive())
				{
					HandlesToRestart.AddUnique(Spec.Handle);
					continue;
				}

				// 이미 실행 중인 어빌리티에는 입력 눌림만 전달한다. (콤보 입력 등)
				const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
				UE_LOG(LogODAbility, Verbose, TEXT("입력 %s: 실행 중인 %s에 전달 (ActiveCount %d, 인스턴스 활성 %d)"),
					*InputTag.ToString(), *GetNameSafe(Spec.Ability), Spec.ActiveCount, Instance ? Instance->IsActive() : -1);
				AbilitySpecInputPressed(Spec);
				continue;
			}

			// 공격 중에 파생 가능한 공격 입력이 들어오면 진행 중인 콤보에 알린다. (콤보 입력 구간에서 콤보가 직접 넘어간다)
			if (!bCanCancel && ODAbility && ODAbility->CanActivateAsComboBranch() && HasMatchingGameplayTag(ODGameplayTags::State_Attacking))
			{
				bSendBranchInput = true;
				continue;
			}

			if (ODAbility && ODAbility->GetActivationPolicy() != EODAbilityActivationPolicy::OnSpawn)
			{
				HandlesToActivate.AddUnique(Spec.Handle);
				if (bCanCancel)
				{
					HandlesToRestart.AddUnique(Spec.Handle);
				}
			}
		}
	}

	// 캔슬 구간에서 회피를 눌렀으면 진행 중인 행동(Ability.Action)을 먼저 끊는다. 상태 태그가 풀려 회피가 막히지 않는다.
	if (!HandlesToRestart.IsEmpty())
	{
		CancelActionAbilities();
		for (const FGameplayAbilitySpecHandle& Handle : HandlesToRestart)
		{
			HandlesToActivate.AddUnique(Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : HandlesToActivate)
	{
		if (!TryActivateAbility(Handle))
		{
			// 실패 원인 기록 (log LogODAbility Verbose 로 확인) + HUD 알림
			if (const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle); Spec && Spec->Ability)
			{
				FGameplayTagContainer FailureTags;
				const bool bCanActivate = Spec->Ability->CanActivateAbility(Handle, AbilityActorInfo.Get(), nullptr, nullptr, &FailureTags);
				const bool bCostOk = Spec->Ability->CheckCost(Handle, AbilityActorInfo.Get());
				FGameplayTagContainer OwnedTags;
				GetOwnedGameplayTags(OwnedTags);
				UE_LOG(LogODAbility, Verbose, TEXT("입력 %s: %s 발동 실패 (CanActivate %d, 비용 %d, 실패 태그 [%s], 보유 태그 [%s], 차단된 어빌리티 태그 [%s])"),
					*InputTag.ToString(), *GetNameSafe(Spec->Ability), bCanActivate, bCostOk, *FailureTags.ToStringSimple(), *OwnedTags.ToStringSimple(), *BlockedAbilityTags.GetExplicitGameplayTags().ToStringSimple());

				FString Reason;
				const UODGameplayAbility* ODAbility = Cast<UODGameplayAbility>(Spec->Ability);
				const FGameplayTagContainer BlockingTags = ODAbility ? ODAbility->GetCurrentBlockingTags(*this) : FGameplayTagContainer();
				if (!BlockingTags.IsEmpty())
				{
					Reason = FString::Printf(TEXT("막힘: %s"), *BlockingTags.ToStringSimple());
				}
				else if (!bCostOk)
				{
					Reason = TEXT("스태미나 부족");
				}
				else
				{
					Reason = TEXT("발동 조건 불충족");
				}
				ReportActivationFailure(Spec->Ability, Reason);
			}
		}
	}

	if (bSendBranchInput)
	{
		FGameplayEventData Payload;
		Payload.EventTag = ODGameplayTags::Event_Combo_BranchInput;
		Payload.InstigatorTags.AddTag(InputTag);
		HandleGameplayEvent(Payload.EventTag, &Payload);
	}
}

bool UODAbilitySystemComponent::TryActivateComboBranch(const FGameplayTag& InputTag)
{
	FGameplayAbilitySpecHandle BranchHandle;
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		const UODGameplayAbility* ODAbility = Cast<UODGameplayAbility>(Spec.Ability);
		if (ODAbility && ODAbility->CanActivateAsComboBranch() && !Spec.IsActive() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			BranchHandle = Spec.Handle;
			break;
		}
	}

	if (!BranchHandle.IsValid())
	{
		UE_LOG(LogODAbility, Verbose, TEXT("콤보 파생 %s: 대상 어빌리티 없음"), *InputTag.ToString());
		return false;
	}

	// 진행 중인 콤보를 끊어 상태 태그(State.Attacking)를 풀고, 파생 발동임을 알리는 태그를 발동 동안만 붙인다.
	CancelActionAbilities();
	AddLooseGameplayTag(ODGameplayTags::State_ComboBranch);
	const bool bActivated = TryActivateAbility(BranchHandle);
	SetLooseGameplayTagCount(ODGameplayTags::State_ComboBranch, 0);

	UE_LOG(LogODAbility, Verbose, TEXT("콤보 파생 %s: %s"), *InputTag.ToString(), bActivated ? TEXT("발동") : TEXT("실패"));
	return bActivated;
}

void UODAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;
		if (Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
		}
	}
}

void UODAbilitySystemComponent::ClearAbilityInput()
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.InputPressed)
		{
			continue;
		}

		Spec.InputPressed = false;
		if (Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
		}
	}
}

void UODAbilitySystemComponent::ReportActivationFailure(const UGameplayAbility* Ability, const FString& Reason)
{
	OnAbilityActivationFailed.Broadcast(Ability, Reason);
}

bool UODAbilitySystemComponent::TryActivateAbilityByInputTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	FGameplayAbilitySpecHandle FoundHandle;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
		{
			if (Spec.Ability && !Spec.IsActive() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				FoundHandle = Spec.Handle;
				break;
			}
		}
	}
	return FoundHandle.IsValid() && TryActivateAbility(FoundHandle);
}

bool UODAbilitySystemComponent::IsAbilityActiveByInputTag(const FGameplayTag& InputTag) const
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return true;
		}
	}
	return false;
}

void UODAbilitySystemComponent::CancelActionAbilities()
{
	const FGameplayTagContainer ActionTags(ODGameplayTags::Ability_Action);
	CancelAbilities(&ActionTags);

	// 끊긴 어빌리티가 정리하지 못한 경우를 대비해 캔슬 구간 태그를 확실히 지운다.
	SetLooseGameplayTagCount(ODGameplayTags::State_Cancelable, 0);
}
