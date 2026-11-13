// OnlyDungeon 어빌리티 시스템 컴포넌트

#include "ODAbilitySystemComponent.h"

#include "AbilitySystem/ODGameplayAbility.h"
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

	// 캔슬 구간: 지금 행동을 끊고 새 행동을 발동할 수 있다.
	const bool bCanCancel = HasMatchingGameplayTag(ODGameplayTags::State_Cancelable);

	// 순회 중에는 어빌리티 목록이 바뀌면 안 되므로, 발동할 핸들을 모아 두었다가 순회가 끝난 뒤 발동한다.
	TArray<FGameplayAbilitySpecHandle> HandlesToActivate;
	TArray<FGameplayAbilitySpecHandle> HandlesToRestart;
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

			if (Spec.IsActive())
			{
				// 캔슬 구간에서 같은 행동을 다시 누르면 끊고 새로 발동한다. (콤보처럼 직접 처리하는 어빌리티는 제외)
				if (bCanCancel && ODAbility && !ODAbility->HandlesInputWhileActive())
				{
					HandlesToRestart.AddUnique(Spec.Handle);
					continue;
				}

				// 이미 실행 중인 어빌리티에는 입력 눌림만 전달한다. (콤보 입력 등)
				AbilitySpecInputPressed(Spec);
				continue;
			}

			if (ODAbility && ODAbility->GetActivationPolicy() != EODAbilityActivationPolicy::OnSpawn)
			{
				HandlesToActivate.AddUnique(Spec.Handle);
			}
		}
	}

	// 캔슬 구간이면 진행 중인 행동(Ability.Action)을 먼저 끊는다. 상태 태그가 풀려 새 행동이 막히지 않는다.
	if (bCanCancel && (!HandlesToActivate.IsEmpty() || !HandlesToRestart.IsEmpty()))
	{
		CancelActionAbilities();
		HandlesToActivate.Append(HandlesToRestart);
	}

	for (const FGameplayAbilitySpecHandle& Handle : HandlesToActivate)
	{
		if (!TryActivateAbility(Handle))
		{
			UE_LOG(LogODAbility, Verbose, TEXT("입력 %s: 어빌리티 발동 조건 불충족"), *InputTag.ToString());
		}
	}
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

void UODAbilitySystemComponent::CancelActionAbilities()
{
	const FGameplayTagContainer ActionTags(ODGameplayTags::Ability_Action);
	CancelAbilities(&ActionTags);

	// 끊긴 어빌리티가 정리하지 못한 경우를 대비해 캔슬 구간 태그를 확실히 지운다.
	SetLooseGameplayTagCount(ODGameplayTags::State_Cancelable, 0);
}
