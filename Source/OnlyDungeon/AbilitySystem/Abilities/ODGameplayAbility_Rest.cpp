// OnlyDungeon 휴식 어빌리티 (세이브 포인트)

#include "ODGameplayAbility_Rest.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameMode.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponActor.h"
#include "World/ODRespawnPoint.h"

UODGameplayAbility_Rest::UODGameplayAbility_Rest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(ODGameplayTags::Ability_Item);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(ODGameplayTags::State_Resting);
	ActivationOwnedTags.AddTag(ODGameplayTags::State_Invulnerable);
	BlockAbilitiesWithTag.AddTag(ODGameplayTags::Ability_Action);
	BlockAbilitiesWithTag.AddTag(ODGameplayTags::Ability_Item);

	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dodging);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Defending);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Drinking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Resting);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ODGameplayTags::Event_Rest;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UODGameplayAbility_Rest::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	AODRespawnPoint* Point = TriggerEventData ? const_cast<AODRespawnPoint*>(Cast<AODRespawnPoint>(TriggerEventData->OptionalObject.Get())) : nullptr;
	if (!IsValid(Character) || !IsValid(Point) || !IsValid(RestMontage) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 휴식을 시작하지 못했습니다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	RestPoint = Point;
	bRestApplied = false;

	// 제자리에 멈춰 불 쪽을 바라본다.
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement(); IsValid(Movement))
	{
		Movement->StopMovementImmediately();
	}
	const FVector ToPoint = (Point->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
	if (!ToPoint.IsNearlyZero())
	{
		Character->SetActorRotation(ToPoint.Rotation());
	}

	// 맨손으로 쉬는 모션이므로 무기를 숨긴다.
	if (const UODEquipmentComponent* Equipment = Character->FindComponentByClass<UODEquipmentComponent>())
	{
		if (AODWeaponActor* Weapon = Equipment->GetWeaponActor(); IsValid(Weapon) && !Weapon->IsHidden())
		{
			Weapon->SetActorHiddenInGame(true);
			HiddenWeapon = Weapon;
		}
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, RestMontage, MontagePlayRate);
	Task->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	Task->ReadyForActivation();

	Character->GetWorldTimerManager().SetTimer(ApplyTimer, this, &ThisClass::ApplyRest, FMath::Max(RestApplyTime, 0.01f), false);
}

void UODGameplayAbility_Rest::ApplyRest()
{
	if (bRestApplied || !IsActive())
	{
		return;
	}
	bRestApplied = true;

	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	const UWorld* World = GetWorld();
	if (AODGameMode* GameMode = World ? World->GetAuthGameMode<AODGameMode>() : nullptr; GameMode && IsValid(Character))
	{
		GameMode->RestAtRespawnPoint(RestPoint.Get(), Character);
	}
}

void UODGameplayAbility_Rest::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UODGameplayAbility_Rest::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 모션이 일찍 끝나도 휴식은 적용한다.
	if (!bWasCancelled)
	{
		ApplyRest();
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyTimer);
	}
	if (AActor* Weapon = HiddenWeapon.Get())
	{
		Weapon->SetActorHiddenInGame(false);
	}
	HiddenWeapon.Reset();
	RestPoint.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
