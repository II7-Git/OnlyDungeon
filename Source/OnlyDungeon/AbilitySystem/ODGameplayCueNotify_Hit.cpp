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
#include "Sound/SoundBase.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

bool UODGameplayCueNotify_Hit::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const AODCharacterBase* Character = Cast<AODCharacterBase>(MyTarget);
	if (!IsValid(Character) || (!IsValid(Character->GetHitImpactEffect()) && !IsValid(Character->GetHitImpactSound())))
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

	// 소리: 거리 감쇠·동시 재생 제한은 사운드 에셋에 지정된 설정을 따른다.
	if (USoundBase* Sound = Character->GetHitImpactSound(); IsValid(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Sound, Location);
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
