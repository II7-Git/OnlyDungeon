// OnlyDungeon 차징 투사체 어빌리티 (보스 불꽃 투사체 패턴)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_ChargedProjectile.generated.h"

class AODFireProjectile;
class UAnimMontage;
class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/** 차징하는 동안 몸에 붙여 키우는 연출 이펙트 하나 */
USTRUCT(BlueprintType)
struct FODChargeEffect
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball")
	TObjectPtr<UNiagaraSystem> System;

	/** 발사 소켓 기준 위치 (소켓 로컬 공간) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball")
	FVector Offset = FVector::ZeroVector;

	/**
	 * 크기를 키울 이펙트 사용자 변수 (예: User.Size). 로컬 공간 스프라이트처럼 컴포넌트 크기를 따르지 않는 이펙트에 쓴다.
	 * 비우면 컴포넌트 크기(Scale)로 키운다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball")
	FName SizeParameter;

	/** 차징 시작·끝 크기. 차징하는 동안 이 사이로 커진다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball", meta = (ClampMin = "0.01"))
	float StartScale = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball", meta = (ClampMin = "0.01"))
	float EndScale = 1.0f;
};

/**
 * 몸의 페이즈 불꽃(AODEnemyCharacter::GetPhaseLoopEffect)을 ChargeTime 동안 키우며 대상 쪽으로 몸을 돌린 뒤,
 * 그 순간의 대상 발밑으로 불꽃 투사체(AODFireProjectile)를 쏜다. 투사체가 터지면 반경 안에 피해를 준다.
 * - 패턴 데이터(DA_Pattern_*)에서 쿨타임과 "2페이즈 진입 시 바로 한 번"(bUseOnceWhenTagsGained)을 정한다.
 * - 차징 중 경직·사망·초기화로 끊기면 쏘지 않고, 불꽃 크기·타이머를 모두 되돌린다.
 * - 페이즈 불꽃이 없으면 크기 조절만 건너뛰고 패턴은 그대로 동작한다.
 * 몽타주·투사체·수치는 BP(GA_*_Fireball)에서 정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_ChargedProjectile : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_ChargedProjectile(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 차징·발사 동안 재생할 몽타주 (발사 후 RecoveryTime이 지나면 멈춘다) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball")
	TObjectPtr<UAnimMontage> ChargeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball", meta = (ClampMin = "0.05"))
	float MontagePlayRate = 1.0f;

	/** 불꽃을 모으는 시간. 끝나면 발사한다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball", meta = (ClampMin = "0.1", Units = "s"))
	float ChargeTime = 2.0f;

	/** 발사 뒤 어빌리티가 끝날 때까지의 시간 (후딜) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball", meta = (ClampMin = "0.0", Units = "s"))
	float RecoveryTime = 0.8f;

	/** 차징이 끝날 때 몸 불꽃의 크기 배율 (원래 크기 1에서 점점 커진다) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball", meta = (ClampMin = "1.0"))
	float ChargeEffectMaxScale = 2.5f;

	/** 차징하는 동안 발사 소켓에 붙여 함께 키우는 이펙트 (불덩이가 모이는 연출). 발사·끊김 때 사라진다. 투사체는 첫 이펙트 자리에서 나간다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball", meta = (TitleProperty = "System"))
	TArray<FODChargeEffect> ChargeEffects;

	/** 차징 중 대상 쪽으로 도는 속도. 발사 순간 방향이 고정된다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball", meta = (ClampMin = "0.0", Units = "deg"))
	float TurnRate = 90.0f;

	/** 투사체 BP */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Projectile")
	TSubclassOf<AODFireProjectile> ProjectileClass;

	/** 투사체가 나가는 소켓(본). 몸 불꽃과 같은 곳에 둔다. 비우면 몸 중심 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Projectile")
	FName LaunchSocket;

	/** 착탄까지 걸리는 시간 (포물선) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Projectile", meta = (ClampMin = "0.1", Units = "s"))
	float FlightTime = 1.0f;

	/** 폭발 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Projectile", meta = (ClampMin = "10.0", Units = "cm"))
	float ImpactRadius = 300.0f;

	/** 피해 GE (SetByCaller Data.Damage / Data.PoiseDamage) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Damage", meta = (ClampMin = "0.0"))
	float Damage = 35.0f;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Damage", meta = (ClampMin = "0.0"))
	float PoiseDamage = 40.0f;

	/** 차징을 시작할 때 몸에서 내는 소리 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Audio")
	TObjectPtr<USoundBase> ChargeSound;

	/** 발사 소리 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Fireball|Audio")
	TObjectPtr<USoundBase> LaunchSound;

private:
	/** 차징 한 단계 (짧은 반복 타이머): 불꽃 키우기, 대상 쪽으로 돌기 */
	void TickCharge();

	/** 차징 끝: 투사체 발사 */
	void Fire();

	/** 후딜 끝 */
	void FinishRecovery();

	/** 몸 불꽃 크기를 원래대로, 차징 연출 이펙트는 끈다. */
	void RestoreChargeEffect();

	UFUNCTION()
	void OnMontageInterrupted();

	/** 크기를 바꾼 몸 불꽃과 원래 크기 */
	TWeakObjectPtr<UNiagaraComponent> ChargeEffect;
	FVector ChargeEffectBaseScale = FVector::OneVector;

	/** 차징 연출로 띄운 이펙트 (ChargeEffects와 같은 순서). 수가 적어 인라인 배열로 둔다. */
	TArray<TWeakObjectPtr<UNiagaraComponent>, TInlineAllocator<4>> SpawnedChargeEffects;

	double ChargeStartTime = 0.0;
	bool bFired = false;

	FTimerHandle ChargeTickTimer;
	FTimerHandle FireTimer;
	FTimerHandle RecoveryTimer;
};
