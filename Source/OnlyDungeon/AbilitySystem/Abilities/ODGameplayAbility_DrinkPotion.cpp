// OnlyDungeon 회복 물약 마시기 어빌리티

#include "ODGameplayAbility_DrinkPotion.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameplayEffect.h"
#include "TimerManager.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "Character/ODCharacterBase.h"
#include "Character/ODPotionComponent.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponActor.h"

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

	// 무기와 관계없이 같은 마시기 모션을 쓰므로 마시는 동안 무기를 숨긴다.
	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		if (const UODEquipmentComponent* Equipment = Avatar->FindComponentByClass<UODEquipmentComponent>())
		{
			if (AODWeaponActor* Weapon = Equipment->GetWeaponActor(); IsValid(Weapon) && !Weapon->IsHidden())
			{
				Weapon->SetActorHiddenInGame(true);
				HiddenWeapon = Weapon;
			}
		}
	}

	// 손에 물약 병을 쥐여 준다. (마시는 동안만 존재하는 컴포넌트)
	if (AODCharacterBase* Character = GetODCharacterFromActorInfo(); IsValid(Character) && IsValid(PotionMesh) && IsValid(Character->GetMesh()))
	{
		UStaticMeshComponent* Bottle = NewObject<UStaticMeshComponent>(Character, NAME_None, RF_Transient);
		Bottle->SetStaticMesh(PotionMesh);
		Bottle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bottle->SetGenerateOverlapEvents(false);
		Bottle->SetupAttachment(Character->GetMesh(), PotionAttachSocket);
		Bottle->SetRelativeTransform(PotionAttachOffset);
		Bottle->RegisterComponent();
		PotionMeshComponent = Bottle;
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

	// 회복이 들어오기 시작하는 순간 몸에 회복 이펙트를 띄운다. (중복 방지: 이미 떠 있으면 그대로 둔다)
	const AODCharacterBase* Character = GetODCharacterFromActorInfo();
	if (IsValid(HealVisualEffect) && !HealVisualComponent.IsValid() && IsValid(Character) && IsValid(Character->GetMesh()))
	{
		HealVisualComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(HealVisualEffect, Character->GetMesh(), HealVisualSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			FVector(HealVisualScale), EAttachLocation::SnapToTarget, true, ENCPoolMethod::None);
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

	// 새 입자만 멈추고 남은 입자는 자연스럽게 사라지게 한다. (SpawnSystemAttached의 자동 제거)
	if (UNiagaraComponent* Effect = HealVisualComponent.Get())
	{
		Effect->Deactivate();
	}
	HealVisualComponent.Reset();
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

	if (AActor* Weapon = HiddenWeapon.Get())
	{
		Weapon->SetActorHiddenInGame(false);
	}
	HiddenWeapon.Reset();

	if (UStaticMeshComponent* Bottle = PotionMeshComponent.Get())
	{
		Bottle->DestroyComponent();
	}
	PotionMeshComponent.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
