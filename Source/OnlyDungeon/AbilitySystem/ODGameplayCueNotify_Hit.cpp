// OnlyDungeon 피격 이펙트 GameplayCue

#include "ODGameplayCueNotify_Hit.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Sound/SoundBase.h"
#include "Character/ODCharacterBase.h"
#include "Character/ODPlayerCharacter.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

bool UODGameplayCueNotify_Hit::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const AODCharacterBase* Character = Cast<AODCharacterBase>(MyTarget);
	if (!IsValid(Character) || (!IsValid(Character->GetHitImpactEffect()) && !IsValid(Character->GetHitImpactSound()) && !IsValid(Character->GetHitGroundEffect())))
	{
		return false;
	}

	// 패링으로 막은 공격은 피해가 없으니 피격 이펙트를 내지 않는다.
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MyTarget); IsValid(ASC) && ASC->HasMatchingGameplayTag(ODGameplayTags::State_Parrying))
	{
		return false;
	}

	// 위치: 무기 판정이 맞은 지점. 없으면 몸 중심.
	FVector Location = MyTarget->GetActorLocation();
	const FHitResult* Hit = Parameters.EffectContext.GetHitResult();
	if (Hit && !Hit->ImpactPoint.IsNearlyZero())
	{
		Location = Hit->ImpactPoint;
	}

	// 방향: 공격자에게서 멀어지는 쪽 (맞아서 튀는 방향)
	FVector Direction = -MyTarget->GetActorForwardVector();
	if (const AActor* Instigator = Parameters.EffectContext.GetInstigator(); IsValid(Instigator))
	{
		Direction = (Location - Instigator->GetActorLocation()).GetSafeNormal2D();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::UpVector;
	}

	// 타격감: 맞은 캐릭터와 공격자를 잠깐 함께 멈추고, 플레이어가 관련되면 화면을 짧게 흔든다.
	AODCharacterBase* TargetCharacter = Cast<AODCharacterBase>(MyTarget);
	AODCharacterBase* InstigatorCharacter = Cast<AODCharacterBase>(Parameters.EffectContext.GetInstigator());
	const float HitStop = Character->GetHitStopDuration();
	TargetCharacter->ApplyHitStop(HitStop);
	if (IsValid(InstigatorCharacter))
	{
		InstigatorCharacter->ApplyHitStop(HitStop);
	}
	if (AODPlayerCharacter* PlayerTarget = Cast<AODPlayerCharacter>(TargetCharacter))
	{
		PlayerTarget->PlayHitCameraShake(false);
	}
	else if (AODPlayerCharacter* PlayerAttacker = Cast<AODPlayerCharacter>(InstigatorCharacter))
	{
		PlayerAttacker->PlayHitCameraShake(true);
	}

	// 소리: 거리 감쇠·동시 재생 제한은 사운드 에셋에 지정된 설정을 따른다.
	if (USoundBase* Sound = Character->GetHitImpactSound(); IsValid(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Sound, Location);
	}

	// 바닥 이펙트: 발밑(캡슐 바닥)에 공격 반대 방향으로 눕혀 재생한다. 한 번 재생형 에셋이라 따로 멈추지 않는다.
	if (UNiagaraSystem* GroundEffect = Character->GetHitGroundEffect(); IsValid(GroundEffect))
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const float HalfHeight = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
		const FVector GroundLocation = MyTarget->GetActorLocation() - FVector(0.0, 0.0, HalfHeight);
		const FRotator GroundRotation(0.0, Direction.GetSafeNormal2D().Rotation().Yaw, 0.0);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(MyTarget, GroundEffect, GroundLocation, GroundRotation, FVector(Character->GetHitGroundScale()));
	}

	if (!IsValid(Character->GetHitImpactEffect()))
	{
		return true;
	}
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(MyTarget, Character->GetHitImpactEffect(), Location, Direction.Rotation(), FVector(Character->GetHitImpactScale()));

	// 반복 재생용으로 만들어진 이펙트(스파크 팩 등)도 한 번 튀고 끝나도록, 짧게 뿜은 뒤 새 입자 생성을 멈춘다.
	// 이미 나온 입자는 수명대로 사라지고, 끝나면 자동 제거된다. (한 번만 터지는 이펙트에는 영향 없음)
	if (IsValid(Effect) && IsValid(MyTarget->GetWorld()))
	{
		constexpr float EmitTime = 0.15f;
		FTimerHandle Handle;
		MyTarget->GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Effect, [Effect]()
		{
			Effect->Deactivate();
		}), EmitTime, false);
	}
	UE_LOG(LogODCombat, Verbose, TEXT("%s 피격 이펙트: %s"), *GetNameSafe(MyTarget), *GetNameSafe(Character->GetHitImpactEffect()));
	return true;
}
