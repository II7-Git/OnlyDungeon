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
 * - 마지막 소모 후 RegenDelay(기본 1.5초)가 지나면 회복 GE(무한 지속, 주기 적용)를 건다.
 * - 스태미나가 가득 차면 회복 GE를 제거한다.
 * - 회복량은 회복 GE가 StaminaRegenRate Attribute를 기반으로 계산한다. (Attribute 변경은 GE로만)
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

	/** 회복 GE (무한 지속 + 주기 적용, StaminaRegenRate 기반) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Stamina")
	TSubclassOf<UGameplayEffect> RegenEffectClass;

	/** 마지막 스태미나 소모 후 회복이 시작되기까지 걸리는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Stamina", meta = (ClampMin = "0.0"))
	float RegenDelay = 1.5f;

private:
	/** 스태미나 값이 바뀔 때 */
	void HandleStaminaChanged(const FOnAttributeChangeData& ChangeData);

	/** 회복 대기 타이머를 (다시) 시작한다. 이미 가득 차 있으면 시작하지 않는다. */
	void RestartRegenDelay();

	/** 회복 GE 적용 */
	void StartRegen();

	/** 회복 GE 제거 */
	void StopRegen();

	bool IsStaminaFull() const;

	FTimerHandle RegenDelayHandle;
	FActiveGameplayEffectHandle RegenEffectHandle;
	FDelegateHandle StaminaChangedHandle;
};
