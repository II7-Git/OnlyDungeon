// OnlyDungeon 불꽃 투사체 (보스 원거리 패턴)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "ODFireProjectile.generated.h"

class UCameraShakeBase;
class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USoundBase;
class USphereComponent;

/** 투사체 몸에 겹쳐 붙이는 이펙트 하나 */
USTRUCT(BlueprintType)
struct FODProjectileLayer
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "OD|Projectile")
	TObjectPtr<UNiagaraSystem> System;

	/** 크기. SizeParameter가 있으면 그 사용자 변수(예: User.Size)에, 없으면 컴포넌트 크기에 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Projectile", meta = (ClampMin = "0.01"))
	float Scale = 1.0f;

	/** 컴포넌트 크기를 따르지 않는 이펙트(로컬 공간 스프라이트 등)의 크기 사용자 변수 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Projectile")
	FName SizeParameter;
};

/**
 * 정해진 지점으로 포물선을 그리며 날아가 터지는 불꽃 투사체.
 * - 쏜 어빌리티가 피해 GE 스펙을 넘겨주고(Launch), 터질 때 반경 안의 다른 팀 캐릭터에게 한 번씩 적용한다.
 * - 대상을 따라가지 않는다. 발사 순간의 착탄 지점으로 FlightTime 동안 날아가므로 굴러서 피할 수 있다.
 * - 날아가는 동안 착탄 지점에 경고 이펙트를 띄운다.
 * - 땅·벽·캐릭터에 닿거나 MaxLifetime이 지나면 그 자리에서 터진다. (쏜 보스가 죽어도 그대로 터진다)
 * - 메시 없이 이펙트만으로 몸을 이룬다: BodyEffect + BodyLayers(겹쳐 붙이는 이펙트). 터지면 그 자리에 멈춰 새 입자만 끊고, 남은 불꽃·꼬리가 사라진 뒤 제거된다.
 * 틱하지 않는다. 이동은 투사체 이동 컴포넌트가 한다. 연출 에셋은 BP(BP_Projectile_*)에서 지정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API AODFireProjectile : public AActor
{
	GENERATED_BODY()

public:
	AODFireProjectile();

	/**
	 * 발사한다. 스폰 직후 한 번 부른다.
	 * @param InDamageSpec   폭발에 맞은 대상에게 적용할 피해 스펙 (쏜 캐릭터의 ASC가 만든 것)
	 * @param TargetLocation 떨어질 지점 (바닥 높이)
	 * @param FlightTime     착탄까지 걸리는 시간
	 * @param InRadius       폭발 반경
	 */
	void Launch(const FGameplayEffectSpecHandle& InDamageSpec, const FVector& TargetLocation, float FlightTime, float InRadius);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** 날아가는 불덩이 이펙트 (에셋은 BP에서 지정) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Projectile")
	TObjectPtr<UNiagaraComponent> BodyEffect;

	/** 불덩이 몸에 겹쳐 붙이는 이펙트 (불꽃 핵·불티·꼬리 등). 발사할 때 붙이고 터질 때 떼어 낸다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile", meta = (TitleProperty = "System"))
	TArray<FODProjectileLayer> BodyLayers;

	/** 날아가는 동안 착탄 지점에 띄우는 경고 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile|Warning")
	TObjectPtr<UNiagaraSystem> WarningEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile|Warning", meta = (ClampMin = "0.01"))
	float WarningScale = 1.0f;

	/** 폭발 이펙트 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile|Explosion", meta = (ClampMin = "0.01"))
	float ExplosionScale = 1.0f;

	/** 폭발 이펙트가 새 입자를 내는 시간. 지나면 멈추고 남은 입자만 사라진다. 0이면 이펙트가 알아서 끝남 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile|Explosion", meta = (ClampMin = "0.0", Units = "s"))
	float ExplosionEffectDuration = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile|Explosion")
	TObjectPtr<USoundBase> ExplosionSound;

	/** 폭발 화면 흔들림 (이 반경 밖의 플레이어는 흔들리지 않는다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile|Explosion")
	TSubclassOf<UCameraShakeBase> ExplosionShake;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile|Explosion", meta = (ClampMin = "100.0", Units = "cm"))
	float ExplosionShakeRadius = 2500.0f;

	/** 착탄하지 못해도 이 시간이 지나면 그 자리에서 터진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Projectile", meta = (ClampMin = "0.5", Units = "s"))
	float MaxLifetime = 5.0f;

private:
	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);

	/** 터진다: 반경 안 대상에게 피해, 연출, 제거. 한 번만 실행된다. */
	void Explode(const FVector& Location);

	FGameplayEffectSpecHandle DamageSpec;
	float Radius = 300.0f;
	bool bExploded = false;

	/** 터질 때 몸 이펙트의 새 입자만 멈춘다. (남은 입자는 FadeOutTime 동안 자연스럽게 사라진다) */
	void ReleaseBodyEffects();

	/** 발사할 때 붙인 겹침 이펙트 */
	TArray<TWeakObjectPtr<UNiagaraComponent>, TInlineAllocator<4>> SpawnedLayers;

	/** 착탄 지점 경고 이펙트 (터질 때 끈다) */
	TWeakObjectPtr<UNiagaraComponent> WarningComponent;

	FTimerHandle LifetimeTimer;
};
