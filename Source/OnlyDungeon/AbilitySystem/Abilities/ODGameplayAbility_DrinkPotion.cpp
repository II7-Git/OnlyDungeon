// OnlyDungeon 회복 물약 마시기 어빌리티

#include "ODGameplayAbility_DrinkPotion.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "Character/ODCharacterBase.h"
#include "Character/ODPotionComponent.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

UODGameplayAbility_DrinkPotion::UODGameplayAbility_DrinkPotion(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EODAbilityActivationPolicy::OnInputTriggered;

	// 행동(Ability.Action)이 아니라 아이템 사용이다. 회피의 행동 취소 대상이 아니고, 마시는 동안 행동을 막는다.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(ODGameplayTags::Ability_Item);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(ODGameplayTags::State_Drinking);
	BlockAbilitiesWithTag.AddTag(ODGameplayTags::Ability_Action);

	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dodging);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Defending);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Drinking);
}

UODPotionComponent* UODGameplayAbility_DrinkPotion::GetPotionComponent() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	return IsValid(Avatar) ? Avatar->FindComponentByClass<UODPotionComponent>() : nullptr;
}

bool UODGameplayAbility_DrinkPotion::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 물약이 남아 있어야 마실 수 있다.
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UODPotionComponent* Potion = IsValid(Avatar) ? Avatar->FindComponentByClass<UODPotionComponent>() : nullptr;
	return IsValid(Potion) && Potion->HasCharges();
}

void UODGameplayAbility_DrinkPotion::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UODPotionComponent* Potion = GetPotionComponent();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(Potion) || !IsValid(ASC) || !HealEffectClass || !CommitAbility(Handle, ActorInfo, ActivationInfo) || !Potion->TryConsume())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 이번 한 모금의 총 회복량
	const float MaxHealth = ASC->GetNumericAttribute(UODAttributeSet_Health::GetMaxHealthAttribute());
	RemainingHeal = MaxHealth * Potion->GetHealRatio();
	const int32 TickCount = FMath::Max(FMath::RoundToInt(HealDuration / HealTickInterval), 1);
	HealPerTick = RemainingHeal / TickCount;

	// 피해를 입으면 마시기를 멈춘다.
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UODAttributeSet_Health::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);

	// 마시는 동안 천천히만 걷는다.
	if (const AODCharacterBase* Character = GetODCharacterFromActorInfo(); IsValid(Character) && IsValid(Character->GetCharacterMovement()))
	{
		SavedMaxWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
		Character->GetCharacterMovement()->MaxWalkSpeed = SavedMaxWalkSpeed * MoveSpeedMultiplier;
	}

	if (IsValid(DrinkMontage))
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DrinkMontage, MontagePlayRate);
		MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
		MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
		MontageTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 마시는 몽타주가 없어 모션 없이 회복합니다."), *GetName());
	}

	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		if (HealStartDelay > 0.0f)
		{
			Avatar->GetWorldTimerManager().SetTimer(HealStartHandle, this, &ThisClass::StartHealing, HealStartDelay, false);
		}
		else
		{
			StartHealing();
		}
	}

	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 물약 마시기 시작 (회복 %.0f, 남은 물약 %d)"), *GetName(), RemainingHeal, Potion->GetCharges());
}

void UODGameplayAbility_DrinkPotion::StartHealing()
{
	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		Avatar->GetWorldTimerManager().SetTimer(HealTickHandle, this, &ThisClass::ApplyHealTick, HealTickInterval, true, 0.0f);
	}
}

void UODGameplayAbility_DrinkPotion::ApplyHealTick()
{
	if (RemainingHeal <= KINDA_SMALL_NUMBER)
	{
		StopHealing();
		// 몽타주가 없으면 회복이 끝나는 시점에 끝낸다.
		if (!IsValid(DrinkMontage) && IsActive())
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		return;
	}

	const float Amount = FMath::Min(HealPerTick, RemainingHeal);
	RemainingHeal -= Amount;

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(HealEffectClass, GetAbilityLevel());
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(ODGameplayTags::Data_Heal, Amount);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
	}
}

void UODGameplayAbility_DrinkPotion::StopHealing()
{
	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		Avatar->GetWorldTimerManager().ClearTimer(HealStartHandle);
		Avatar->GetWorldTimerManager().ClearTimer(HealTickHandle);
	}
}

void UODGameplayAbility_DrinkPotion::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	// 피격(체력 감소) 시 마시기를 끊는다. 회복으로 오르는 변화는 무시한다.
	if (ChangeData.NewValue < ChangeData.OldValue && IsActive())
	{
		UE_LOG(LogODAbility, Verbose, TEXT("[%s] 피격으로 마시기 중단 (남은 회복 %.0f 버림)"), *GetName(), RemainingHeal);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UODGameplayAbility_DrinkPotion::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UODGameplayAbility_DrinkPotion::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UODGameplayAbility_DrinkPotion::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopHealing();
	RemainingHeal = 0.0f;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); IsValid(ASC) && HealthChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UODAttributeSet_Health::GetHealthAttribute()).Remove(HealthChangedHandle);
	}
	HealthChangedHandle.Reset();

	if (SavedMaxWalkSpeed >= 0.0f)
	{
		if (const AODCharacterBase* Character = GetODCharacterFromActorInfo(); IsValid(Character) && IsValid(Character->GetCharacterMovement()))
		{
			Character->GetCharacterMovement()->MaxWalkSpeed = SavedMaxWalkSpeed;
		}
		SavedMaxWalkSpeed = -1.0f;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
