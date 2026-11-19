// OnlyDungeon 달리기 어빌리티

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ODGameplayAbility_Sprint.generated.h"

class UGameplayEffect;

/**
 * 달리기: 회피 키를 길게 누르면 플레이어가 발동하고(InputTag.Sprint), 키를 떼면 끝난다.
 * - 달리는 동안 이동 속도를 SprintSpeed로 올리고, 실제로 움직이는 동안 초당 StaminaCostPerSecond만큼 스태미나를 쓴다.
 *   소모는 즉시 적용 GE(DrainEffectClass)의 SetByCaller(Data.StaminaCost)로 넘긴다. (Attribute 변경은 GE로만)
 * - 스태미나가 0이 되면 멈춘다. 다시 달리려면 키를 다시 눌러야 한다. (플레이어 입력 처리)
 * - 스태미나가 계속 줄어드므로 자연 회복 어빌리티는 달리는 동안 회복을 시작하지 않는다.
 * - 공격·회피·방어·물약·경직·휴식·사망 상태가 되면 즉시 끝난다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_Sprint : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_Sprint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 스태미나 소모 GE (즉시 적용, Stamina += SetByCaller Data.StaminaCost). 음수 값을 넘긴다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Sprint")
	TSubclassOf<UGameplayEffect> DrainEffectClass;

	/** 달리기 이동 속도 (cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Sprint", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SprintSpeed = 750.0f;

	/** 움직이는 동안 초당 스태미나 소모량 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Sprint", meta = (ClampMin = "0.0"))
	float StaminaCostPerSecond = 12.0f;

	/** 달리기를 시작하려면 필요한 최소 스태미나 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Sprint", meta = (ClampMin = "0.0"))
	float MinStaminaToStart = 5.0f;

	/** 이 속도보다 느리면(제자리) 스태미나를 쓰지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Sprint", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinMoveSpeedToDrain = 50.0f;

	/** 소모 틱 간격(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Sprint", meta = (ClampMin = "0.02", Units = "s"))
	float TickInterval = 0.1f;

private:
	/** 소모 한 틱: 움직이는 중이면 스태미나를 쓰고, 바닥나면 끝낸다. */
	void ApplyDrainTick();

	/** 끝내야 하는 상태 태그가 붙었을 때 */
	void HandleBlockingTagChanged(const FGameplayTag Tag, int32 NewCount);

	FTimerHandle DrainTimer;
	TArray<TPair<FGameplayTag, FDelegateHandle>> TagEventHandles;
	float SavedMaxWalkSpeed = -1.0f;
};
