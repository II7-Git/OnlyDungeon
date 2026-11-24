// OnlyDungeon 차징 투사체 어빌리티

#include "ODGameplayAbility_ChargedProjectile.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "AbilitySystem/ODFireProjectile.h"
#include "Character/ODEnemyCharacter.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

namespace ODChargedProjectile
{
	/** 차징 중 불꽃 크기·몸 방향 갱신 간격 (약 30fps면 커지는 모습이 끊겨 보이지 않는다) */
	constexpr float ChargeTickInterval = 1.0f / 30.0f;
}

UODGameplayAbility_ChargedProjectile::UODGameplayAbility_ChargedProjectile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FGameplayTagContainer Tags;
	Tags.AddTag(ODGameplayTags::Ability_Action);
	Tags.AddTag(ODGameplayTags::Ability_Boss_Fireball);
	SetAssetTags(Tags);

	// 공격 중으로 취급해 AI가 다른 행동을 시작하지 않는다. 죽었거나 경직 중이면 쓰지 않는다.
	ActivationOwnedTags.AddTag(ODGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Attacking);
}

void UODGameplayAbility_ChargedProjectile::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !World || !ProjectileClass || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bFired = false;
	ChargeStartTime = World->GetTimeSeconds();

	// 몸 불꽃을 키울 준비 (적이 아니거나 페이즈 불꽃이 없으면 크기 조절 없이 진행)
	ChargeEffect.Reset();
	if (const AODEnemyCharacter* Enemy = Cast<AODEnemyCharacter>(Character))
	{
		if (UNiagaraComponent* Effect = Enemy->GetPhaseLoopEffect(); IsValid(Effect))
		{
			ChargeEffect = Effect;
			ChargeEffectBaseScale = Effect->GetRelativeScale3D();
		}
	}

	if (IsValid(ChargeMontage))
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ChargeMontage, MontagePlayRate);
		Task->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
		Task->OnCancelled.AddDynamic(this, &ThisClass::OnMontageInterrupted);
		Task->ReadyForActivation();
	}

	if (IsValid(ChargeSound) && IsValid(Character->GetMesh()))
	{
		UGameplayStatics::SpawnSoundAttached(ChargeSound, Character->GetMesh(), LaunchSocket, FVector::ZeroVector, EAttachLocation::SnapToTarget, true);
	}

	FTimerManager& Timers = World->GetTimerManager();
	Timers.SetTimer(ChargeTickTimer, this, &ThisClass::TickCharge, ODChargedProjectile::ChargeTickInterval, true);
	Timers.SetTimer(FireTimer, this, &ThisClass::Fire, ChargeTime, false);

	UE_LOG(LogODAbility, Log, TEXT("%s 불꽃 차징 시작 (%.1f초)"), *GetNameSafe(Character), ChargeTime);
}

void UODGameplayAbility_ChargedProjectile::TickCharge()
{
	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	const UWorld* World = GetWorld();
	if (!IsActive() || !IsValid(Character) || !World)
	{
		return;
	}

	const float Alpha = FMath::Clamp(static_cast<float>(World->GetTimeSeconds() - ChargeStartTime) / ChargeTime, 0.0f, 1.0f);
	if (UNiagaraComponent* Effect = ChargeEffect.Get())
	{
		Effect->SetRelativeScale3D(ChargeEffectBaseScale * FMath::Lerp(1.0f, ChargeEffectMaxScale, Alpha));
	}

	// 대상 쪽으로 천천히 돈다. (발사 순간 고정되므로 마지막까지 따라오되 급회전은 하지 않는다)
	if (const AActor* Target = Character->GetLockOnTarget(); IsValid(Target) && TurnRate > 0.0f)
	{
		const FVector ToTarget = (Target->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator Current = Character->GetActorRotation();
			const FRotator Desired(0.0, ToTarget.Rotation().Yaw, 0.0);
			Character->SetActorRotation(FMath::RInterpConstantTo(Current, Desired, ODChargedProjectile::ChargeTickInterval, TurnRate));
		}
	}
}

void UODGameplayAbility_ChargedProjectile::Fire()
{
	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsActive() || !IsValid(Character) || !World || bFired)
	{
		return;
	}
	bFired = true;
	World->GetTimerManager().ClearTimer(ChargeTickTimer);
	RestoreChargeEffect();

	// 착탄 지점: 지금 대상의 발밑. 대상이 없으면 쏘지 않고 후딜만 한다.
	const AActor* Target = Character->GetLockOnTarget();
	if (IsValid(Target))
	{
		FVector TargetLocation = Target->GetActorLocation();
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(Target); IsValid(TargetCharacter) && IsValid(TargetCharacter->GetCapsuleComponent()))
		{
			TargetLocation.Z -= TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}

		USkeletalMeshComponent* Mesh = Character->GetMesh();
		const FVector LaunchLocation = IsValid(Mesh) && !LaunchSocket.IsNone() ? Mesh->GetSocketLocation(LaunchSocket) : Character->GetActorLocation();

		FActorSpawnParameters Params;
		Params.Owner = Character;
		Params.Instigator = Character;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AODFireProjectile* Projectile = World->SpawnActor<AODFireProjectile>(ProjectileClass, LaunchLocation, (TargetLocation - LaunchLocation).Rotation(), Params);
		if (IsValid(Projectile))
		{
			FGameplayEffectSpecHandle Spec;
			if (DamageEffectClass)
			{
				Spec = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
				if (Spec.IsValid())
				{
					Spec.Data->SetSetByCallerMagnitude(ODGameplayTags::Data_Damage, Damage);
					Spec.Data->SetSetByCallerMagnitude(ODGameplayTags::Data_PoiseDamage, PoiseDamage);
				}
			}
			Projectile->Launch(Spec, TargetLocation, FlightTime, ImpactRadius);
		}

		if (IsValid(LaunchSound))
		{
			UGameplayStatics::PlaySoundAtLocation(Character, LaunchSound, LaunchLocation);
		}
		UE_LOG(LogODAbility, Log, TEXT("%s 불꽃 투사체 발사 → %s"), *GetNameSafe(Character), *GetNameSafe(Target));
	}
	else
	{
		UE_LOG(LogODAbility, Verbose, TEXT("%s 불꽃 투사체: 대상이 없어 쏘지 않음"), *GetNameSafe(Character));
	}

	World->GetTimerManager().SetTimer(RecoveryTimer, this, &ThisClass::FinishRecovery, FMath::Max(RecoveryTime, KINDA_SMALL_NUMBER), false);
}

void UODGameplayAbility_ChargedProjectile::FinishRecovery()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UODGameplayAbility_ChargedProjectile::OnMontageInterrupted()
{
	// 차징 중에 다른 몽타주(경직 등)에 밀려나면 쏘지 않고 끝낸다. 발사 후라면 후딜 타이머가 끝낸다.
	if (!bFired)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UODGameplayAbility_ChargedProjectile::RestoreChargeEffect()
{
	if (UNiagaraComponent* Effect = ChargeEffect.Get())
	{
		Effect->SetRelativeScale3D(ChargeEffectBaseScale);
	}
	ChargeEffect.Reset();
}

void UODGameplayAbility_ChargedProjectile::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(ChargeTickTimer);
		Timers.ClearTimer(FireTimer);
		Timers.ClearTimer(RecoveryTimer);
	}
	RestoreChargeEffect();
	if (IsActive())
	{
		// 후딜까지 끝났으면 늘려 둔 차징 몽타주를 내린다. (취소된 경우 다른 몽타주가 이미 재생 중일 수 있어 건드리지 않는다)
		if (!bWasCancelled)
		{
			MontageStop();
		}
		if (bWasCancelled && !bFired)
		{
			UE_LOG(LogODAbility, Log, TEXT("%s 불꽃 차징 끊김 (발사 안 함)"), *GetNameSafe(GetAvatarActorFromActorInfo()));
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
