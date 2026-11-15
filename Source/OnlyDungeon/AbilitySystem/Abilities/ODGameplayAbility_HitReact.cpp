// OnlyDungeon 피격 경직 어빌리티

#include "ODGameplayAbility_HitReact.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "GameplayEffect.h"

UODGameplayAbility_HitReact::UODGameplayAbility_HitReact(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 입력이 아니라 강인도 붕괴 이벤트로 발동한다.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = ODGameplayTags::Event_PoiseBroken;
		Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(Trigger);
	}
}

void UODGameplayAbility_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const AActor* Attacker = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
	const bool bParried = TriggerEventData && TriggerEventData->EventMagnitude > 0.0f;
	UAnimMontage* Montage = (bParried && IsValid(ParriedMontage)) ? ParriedMontage.Get() : SelectMontage(Avatar, Attacker);
	if (!IsValid(Montage))
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 경직 몽타주가 지정되지 않았습니다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, 1.0f);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();

	UE_LOG(LogODCombat, Log, TEXT("%s 경직 (%s, 공격자 %s)"), *GetNameSafe(Avatar), *GetNameSafe(Montage), *GetNameSafe(Attacker));
}

void UODGameplayAbility_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 경직이 어떻게 끝나든 강인도는 회복시킨다. (사망으로 취소된 경우 제외)
	if (PoiseRecoverEffectClass && IsActive())
	{
		const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		const bool bDead = IsValid(ASC) && ASC->HasMatchingGameplayTag(ODGameplayTags::State_Dead);
		if (!bDead)
		{
			const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(PoiseRecoverEffectClass, GetAbilityLevel());
			if (SpecHandle.IsValid())
			{
				ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAnimMontage* UODGameplayAbility_HitReact::SelectMontage(const AActor* Avatar, const AActor* Attacker) const
{
	if (!IsValid(Avatar) || !IsValid(Attacker))
	{
		return FrontMontage;
	}

	FVector ToAttacker = Attacker->GetActorLocation() - Avatar->GetActorLocation();
	ToAttacker.Z = 0.0f;
	if (ToAttacker.IsNearlyZero())
	{
		return FrontMontage;
	}
	ToAttacker.Normalize();

	// 정면 기준 공격자 각도: 오른쪽이 양수
	const float Forward = FVector::DotProduct(Avatar->GetActorForwardVector(), ToAttacker);
	const float Right = FVector::DotProduct(Avatar->GetActorRightVector(), ToAttacker);
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Right, Forward));

	if (FMath::Abs(Angle) <= 45.0f)
	{
		return FrontMontage;
	}
	if (FMath::Abs(Angle) >= 135.0f)
	{
		return BackMontage ? BackMontage : FrontMontage;
	}
	if (Angle > 0.0f)
	{
		return RightMontage ? RightMontage : FrontMontage;
	}
	return LeftMontage ? LeftMontage : FrontMontage;
}

void UODGameplayAbility_HitReact::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UODGameplayAbility_HitReact::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
