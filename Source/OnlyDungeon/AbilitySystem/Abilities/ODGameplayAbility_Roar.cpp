// OnlyDungeon 포효 어빌리티

#include "ODGameplayAbility_Roar.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraShakeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

UODGameplayAbility_Roar::UODGameplayAbility_Roar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 포효 중에는 공격 중으로 취급해 AI가 다른 행동을 시작하지 않는다. 진행 중이던 공격·회피는 끊는다.
	ActivationOwnedTags.AddTag(ODGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dead);
	CancelAbilitiesWithTag.AddTag(ODGameplayTags::Ability_Action);
}

void UODGameplayAbility_Roar::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bInvulnerable)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AddLooseGameplayTag(ODGameplayTags::State_Invulnerable);
			bAddedInvulnerable = true;
		}
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(EffectTimer, this, &ThisClass::PlayRoarEffects, FMath::Max(EffectDelay, 0.01f), false);
	}

	if (IsValid(RoarMontage))
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, RoarMontage, MontagePlayRate);
		Task->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
		Task->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
		Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
		Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
		Task->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 포효 몽타주가 없습니다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UE_LOG(LogODAbility, Log, TEXT("%s 포효 (%s)"), *GetNameSafe(GetAvatarActorFromActorInfo()), TriggerEventData ? *TriggerEventData->EventTag.ToString() : TEXT("직접 발동"));
}

void UODGameplayAbility_Roar::PlayRoarEffects()
{
	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	if (!IsActive() || !IsValid(Character))
	{
		return;
	}

	if (CameraShakeClass)
	{
		UGameplayStatics::PlayWorldCameraShake(Character, CameraShakeClass, Character->GetActorLocation(), 0.0f, CameraShakeRadius, 1.0f, false);
	}

	if (IsValid(RoarEffect) && IsValid(Character->GetMesh()))
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(RoarEffect, Character->GetMesh(), RoarEffectSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			FVector(RoarEffectScale), EAttachLocation::SnapToTarget, true, ENCPoolMethod::None);
		if (IsValid(Effect) && RoarEffectDuration > 0.0f)
		{
			FTimerHandle StopHandle;
			Character->GetWorldTimerManager().SetTimer(StopHandle, FTimerDelegate::CreateWeakLambda(Effect, [Effect]()
			{
				Effect->Deactivate();
			}), RoarEffectDuration, false);
		}
	}
}

void UODGameplayAbility_Roar::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UODGameplayAbility_Roar::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EffectTimer);
	}
	if (bAddedInvulnerable)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(ODGameplayTags::State_Invulnerable);
		}
		bAddedInvulnerable = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
