// OnlyDungeon 포효 어빌리티 (보스 등장·페이즈 전환 연출)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_Roar.generated.h"

class UAnimMontage;
class UCameraShakeBase;
class USoundBase;
class UNiagaraSystem;

/**
 * 포효: 게임플레이 이벤트(BP의 Ability Triggers, 예: Event.Boss.Intro / Event.Boss.PhaseChange)로 발동해
 * 하던 행동(Ability.Action)을 끊고 포효 몽타주를 재생한다.
 * - 재생 중에는 State.Attacking이 붙어 AI가 공격·간 보기를 하지 않고, bInvulnerable이면 무적(State.Invulnerable)이다.
 * - 포효 시작 후 EffectDelay에 화면 흔들림(주변 플레이어 거리 감쇠)과 이펙트를 낸다.
 * 연출 수치·몽타주·이펙트는 BP(GA_*_Roar)에서 정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_Roar : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_Roar(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 포효 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar")
	TObjectPtr<UAnimMontage> RoarMontage;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar", meta = (ClampMin = "0.05"))
	float MontagePlayRate = 1.0f;

	/** 포효하는 동안 피해를 받지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar")
	bool bInvulnerable = true;

	/** 포효 시작 후 흔들림·이펙트를 내기까지의 시간 (울부짖는 순간에 맞춘다) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect", meta = (ClampMin = "0.0", Units = "s"))
	float EffectDelay = 0.5f;

	/** 화면 흔들림 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect")
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect", meta = (ClampMin = "0.0"))
	float CameraShakeScale = 1.0f;

	/** 이 거리 밖의 플레이어는 흔들리지 않는다. (안쪽은 거리에 따라 약해짐) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect", meta = (ClampMin = "100.0", Units = "cm"))
	float CameraShakeRadius = 4000.0f;

	/** 포효할 때 터뜨리는 이펙트 (몸에 붙어서 재생) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect")
	TObjectPtr<UNiagaraSystem> RoarEffect;

	/** 이펙트를 붙일 소켓(본). 비우면 몸 중심 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect")
	FName RoarEffectSocket;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect", meta = (ClampMin = "0.01"))
	float RoarEffectScale = 1.0f;

	/** 포효 소리. EffectDelay에 이펙트와 함께 RoarEffectSocket(없으면 메시 중심)에 붙여 재생한다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect")
	TObjectPtr<USoundBase> RoarSound;

	/** 반복형 이펙트를 이 시간 뒤에 멈춘다. (0이면 이펙트가 알아서 끝남) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Roar|Effect", meta = (ClampMin = "0.0", Units = "s"))
	float RoarEffectDuration = 1.2f;

private:
	/** 흔들림·이펙트 */
	void PlayRoarEffects();

	UFUNCTION()
	void OnMontageFinished();

	FTimerHandle EffectTimer;
	bool bAddedInvulnerable = false;
};
