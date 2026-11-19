// OnlyDungeon 달리기 어빌리티

#include "ODGameplayAbility_Sprint.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Combat.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

UODGameplayAbility_Sprint::UODGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 키를 떼면 끝난다. 행동(Ability.Action)이 아니므로 다른 행동을 막지 않고, 행동이 시작되면 스스로 끝난다.
	ActivationPolicy = EODAbilityActivationPolicy::WhileInputActive;

	ActivationOwnedTags.AddTag(ODGameplayTags::State_Sprinting);

	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dodging);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Defending);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Drinking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Resting);
}

bool UODGameplayAbility_Sprint::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return IsValid(ASC) && ASC->GetNumericAttribute(UODAttributeSet_Combat::GetStaminaAttribute()) >= MinStaminaToStart;
}

void UODGameplayAbility_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Movement) || !IsValid(ASC) || !DrainEffectClass)
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 이동 컴포넌트·ASC·소모 GE가 없어 달리기를 시작하지 못했습니다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
	Movement->MaxWalkSpeed = SprintSpeed;

	// 행동이 시작되면(태그가 붙는 순간) 바로 끝낸다. 다른 어빌리티가 이동 속도를 저장하기 전에 원래 속도로 돌려 놓기 위해서다.
	for (const FGameplayTag& Tag : ActivationBlockedTags)
	{
		const FDelegateHandle DelegateHandle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleBlockingTagChanged);
		TagEventHandles.Emplace(Tag, DelegateHandle);
	}

	Character->GetWorldTimerManager().SetTimer(DrainTimer, this, &ThisClass::ApplyDrainTick, TickInterval, true);
	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 달리기 시작 (속도 %.0f → %.0f)"), *GetName(), SavedMaxWalkSpeed, SprintSpeed);
}

void UODGameplayAbility_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); IsValid(ASC))
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : TagEventHandles)
		{
			ASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
		}
	}
	TagEventHandles.Reset();

	if (AODCharacterBase* Character = GetODCharacterFromActorInfo(); IsValid(Character))
	{
		Character->GetWorldTimerManager().ClearTimer(DrainTimer);
		if (SavedMaxWalkSpeed >= 0.0f && IsValid(Character->GetCharacterMovement()))
		{
			Character->GetCharacterMovement()->MaxWalkSpeed = SavedMaxWalkSpeed;
		}
	}
	SavedMaxWalkSpeed = -1.0f;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UODGameplayAbility_Sprint::ApplyDrainTick()
{
	const AODCharacterBase* Character = GetODCharacterFromActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsActive() || !IsValid(Character) || !IsValid(ASC))
	{
		return;
	}

	// 제자리에 서 있으면 스태미나를 쓰지 않는다.
	if (Character->GetVelocity().Size2D() < MinMoveSpeedToDrain)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DrainEffectClass, GetAbilityLevel());
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(ODGameplayTags::Data_StaminaCost, -StaminaCostPerSecond * TickInterval);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
	}

	if (ASC->GetNumericAttribute(UODAttributeSet_Combat::GetStaminaAttribute()) <= 0.0f)
	{
		UE_LOG(LogODAbility, Verbose, TEXT("[%s] 스태미나가 바닥나 달리기 종료"), *GetName());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UODGameplayAbility_Sprint::HandleBlockingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0 && IsActive())
	{
		UE_LOG(LogODAbility, Verbose, TEXT("[%s] %s 상태가 되어 달리기 종료"), *GetName(), *Tag.ToString());
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}
