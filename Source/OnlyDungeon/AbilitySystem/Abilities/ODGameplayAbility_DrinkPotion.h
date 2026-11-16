// OnlyDungeon 회복 물약 마시기 어빌리티

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_DrinkPotion.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UODPotionComponent;
struct FOnAttributeChangeData;

/**
 * 회복 물약 마시기 (R, InputTag.UseItem). 소울류 에스트 병 방식.
 * - 발동하면 물약 하나를 바로 소모하고 마시는 몽타주를 재생한다.
 * - HealStartDelay 뒤부터 HealDuration 동안 나눠서 회복한다. 총 회복량 = 최대 체력 × 물약 회복 비율(기본 60%).
 *   회복은 즉시 적용 GE(HealEffectClass, SetByCaller Data.Heal)를 틱마다 적용한다. (Attribute 변경은 GE로만)
 * - 마시는 도중 피해를 입거나(체력 감소) 경직 등으로 몽타주가 끊기면 즉시 끝나고 회복도 멈춘다. 소모한 물약은 돌아오지 않는다.
 * - 마시는 동안 천천히만 걸을 수 있고(MoveSpeedMultiplier), 공격·회피·방어(Ability.Action)는 막힌다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_DrinkPotion : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_DrinkPotion(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 마시는 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Potion")
	TObjectPtr<UAnimMontage> DrinkMontage;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Potion", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	/** 회복 틱 GE (즉시 적용, Health += SetByCaller Data.Heal) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Potion")
	TSubclassOf<UGameplayEffect> HealEffectClass;

	/** 마시기 시작 후 회복이 시작되기까지(초). 병을 입에 대는 시점 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Potion", meta = (ClampMin = "0.0", Units = "s"))
	float HealStartDelay = 0.4f;

	/** 회복이 나눠 들어오는 시간(초). 몽타주가 끝나기 전에 끝나도록 맞춘다 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Potion", meta = (ClampMin = "0.05", Units = "s"))
	float HealDuration = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Potion", meta = (ClampMin = "0.02", Units = "s"))
	float HealTickInterval = 0.1f;

	/** 마시는 동안 이동 속도 배율 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Potion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveSpeedMultiplier = 0.3f;

private:
	UODPotionComponent* GetPotionComponent() const;

	void StartHealing();
	void ApplyHealTick();
	void StopHealing();
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageInterrupted();

	FTimerHandle HealStartHandle;
	FTimerHandle HealTickHandle;
	FDelegateHandle HealthChangedHandle;

	/** 이번 한 모금으로 아직 남은 회복량 */
	float RemainingHeal = 0.0f;
	float HealPerTick = 0.0f;

	/** 줄이기 전 이동 속도 (끝날 때 복구) */
	float SavedMaxWalkSpeed = -1.0f;

	/** 마시는 동안 숨긴 무기 (끝날 때 다시 보이게). 무기 종류와 관계없이 같은 모션을 쓰기 위해 숨긴다. */
	TWeakObjectPtr<AActor> HiddenWeapon;
};
