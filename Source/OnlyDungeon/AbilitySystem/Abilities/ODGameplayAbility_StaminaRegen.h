// OnlyDungeon 스태미나 자연 회복 패시브 어빌리티

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "ODGameplayAbility_StaminaRegen.generated.h"

class UGameplayEffect;
struct FOnAttributeChangeData;

/**
 * 스태미나 자연 회복 (패시브, 부여 즉시 발동).
 * - 스태미나가 줄어드는 행동(회피, 공격 등)이 있으면 회복을 멈추고 대기 타이머를 다시 시작한다.
 * - 마지막 소모 후 RegenDelay(기본 1.5초)가 지나면 TickInterval마다 회복 틱을 적용한다.
 * - 회복이 이어질수록 빨라진다: 배율이 StartRateMultiplier에서 RampTime에 걸쳐 MaxRateMultiplier까지 오른다.
 *   한 틱 회복량 = StaminaRegenRate × 배율 × TickInterval. 소모가 생기면 배율은 처음으로 돌아간다.
 * - 회복량은 즉시 적용 GE(RegenEffectClass)의 SetByCaller(Data.StaminaRegen)로 넘긴다. (Attribute 변경은 GE로만)
 * - 스태미나가 가득 차면 회복을 멈춘다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_StaminaRegen : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_StaminaRegen(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 회복 틱 GE (즉시 적용, Stamina += SetByCaller Data.StaminaRegen) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Stamina")
	TSubclassOf<UGameplayEffect> RegenEffectClass;

	/** 회복 틱 간격(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Stamina", meta = (ClampMin = "0.02", Units = "s"))
	float TickInterval = 0.1f;

	/** 회복을 시작할 때의 속도 배율 (StaminaRegenRate에 곱함) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Stamina", meta = (ClampMin = "0.0"))
	float StartRateMultiplier = 1.0f;

	/** 회복이 RampTime 이상 이어졌을 때의 최대 속도 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Stamina", meta = (ClampMin = "0.0"))
	float MaxRateMultiplier = 2.0f;

	/** 시작 배율에서 최대 배율까지 오르는 데 걸리는 시간(초). 0이면 바로 최대 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Stamina", meta = (ClampMin = "0.0", Units = "s"))
	float RampTime = 2.0f;

	/** 마지막 스태미나 소모 후 회복이 시작되기까지 걸리는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Stamina", meta = (ClampMin = "0.0"))
	float RegenDelay = 1.5f;

private:
	/** 스태미나 값이 바뀔 때 */
	void HandleStaminaChanged(const FOnAttributeChangeData& ChangeData);

	/** 회복 대기 타이머를 (다시) 시작한다. 이미 가득 차 있으면 시작하지 않는다. */
	void RestartRegenDelay();

	/** 회복 시작 (틱 타이머) */
	void StartRegen();

	/** 회복 정지 */
	void StopRegen();

	/** 회복 한 틱: 지금 배율로 회복 GE를 적용한다. */
	void ApplyRegenTick();

	/** 지금 회복 속도 배율 */
	float GetCurrentRateMultiplier() const;

	bool IsStaminaFull() const;

	FTimerHandle RegenDelayHandle;
	FTimerHandle RegenTickHandle;

	/** 회복이 이어진 시간 (소모·가득 참으로 멈추면 0) */
	float RegenElapsed = 0.0f;
	bool bRegenActive = false;
	FDelegateHandle StaminaChangedHandle;
};
