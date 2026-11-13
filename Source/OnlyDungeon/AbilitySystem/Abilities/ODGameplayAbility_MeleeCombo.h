// OnlyDungeon 근접 콤보 공격 어빌리티 (약공격 등)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_MeleeCombo.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;
class UAnimMontage;
class UGameplayEffect;

/** 콤보 한 타격의 정의 */
USTRUCT(BlueprintType)
struct FODComboStep
{
	GENERATED_BODY()

	/** 이 타격의 몽타주 섹션 이름 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo")
	FName Section;

	/** 기본 피해 (피해 계산에서 공격력이 더해진다) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo", meta = (ClampMin = "0.0"))
	float Damage = 10.0f;

	/** 강인도 피해 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo", meta = (ClampMin = "0.0"))
	float PoiseDamage = 10.0f;

	/** 이 타격에서 정면으로 전진할 거리(cm). 공격 모션이 제자리 모션이라 코드로 한 걸음을 만든다. 0이면 전진하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo|Step", meta = (ClampMin = "0.0", Units = "cm"))
	float StepDistance = 50.0f;

	/** 전진에 걸리는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo|Step", meta = (ClampMin = "0.01", Units = "s"))
	float StepDuration = 0.15f;

	/** 섹션 시작 후 전진을 시작하기까지의 지연(초). 휘두르기 직전 발을 내딛는 타이밍에 맞추는 용도 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo|Step", meta = (ClampMin = "0.0", Units = "s"))
	float StepDelay = 0.0f;
};

/**
 * 몽타주 하나에 섹션으로 나뉜 콤보를 재생하는 근접 공격 어빌리티.
 *
 * 몽타주 구성 (예: AM_OH_LightCombo)
 * - 섹션: A → A_Rec, B → B_Rec, ... (공격 섹션 다음에 후딜 섹션이 이어지고, 후딜이 끝나면 몽타주 종료)
 * - 공격 섹션 끝부분 ~ 후딜 앞부분에 콤보 입력 구간 노티파이(Event.Montage.ComboWindow)를 둔다.
 *
 * 동작
 * - 발동: 스태미나 비용 지불(CommitAbility) → 입력 방향으로 회전 → 첫 공격 섹션 재생
 * - 공격 중 다시 입력하면 예약해 두고, 콤보 입력 구간이 열려 있으면 즉시 다음 공격 섹션으로 넘어간다. (매 타격마다 비용 지불)
 * - 공격 섹션이 시작될 때마다 타격의 StepDistance만큼 정면으로 전진한다. (Root Motion Source라 충돌을 지킨다)
 * - 입력이 없으면 후딜 섹션까지 재생되고 종료된다. 스태미나가 부족하면 다음 타격으로 넘어가지 않는다.
 * - 판정: 공격 섹션의 무기 판정 노티파이(UODAnimNotifyState_WeaponTrace)가 Event.Melee.Hit을 보내면
 *   현재 타격의 피해·강인도 피해를 SetByCaller로 담아 DamageEffectClass를 대상에게 적용한다.
 * - 상태 태그(State.Attacking)와 차단 태그는 BP(GA_*)에서 지정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_MeleeCombo : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_MeleeCombo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 콤보는 실행 중 입력(다음 타격)을 직접 처리한다. */
	virtual bool HandlesInputWhileActive() const override { return true; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** 콤보 몽타주 (섹션으로 나뉜 하나의 몽타주) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo")
	TObjectPtr<UAnimMontage> ComboMontage;

	/** 콤보 타격 목록 (순서대로) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo", meta = (TitleProperty = "Section"))
	TArray<FODComboStep> ComboSteps;

	/** 적중 시 대상에게 적용할 피해 GE (ODDamageExecution 사용) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 몽타주 재생 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	/** 타격마다 입력 방향으로 즉시 회전할지 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo")
	bool bRotateToInput = true;

private:
	/** 다음 공격 섹션으로 넘어간다. 마지막이거나 스태미나가 부족하면 넘어가지 않는다. */
	void TryAdvanceCombo();

	/** 이동 입력이 있으면 그 방향으로 즉시 회전 */
	void RotateToInputDirection() const;

	/** 현재 타격의 전진을 시작한다. 이전 전진이 남아 있으면 끊고 새로 시작한다. */
	void StartStepForward();

	/** 현재 타격의 전진 이동을 적용한다. (지연 타이머에서도 호출) */
	void ApplyStepForce();

	/** 진행 중인 전진 이동 태스크 */
	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> StepTask;

	/** StepDelay 대기 타이머 */
	FTimerHandle StepDelayTimer;

	UFUNCTION()
	void OnComboWindowEvent(FGameplayEventData Payload);

	/** 무기가 대상을 맞혔을 때 */
	UFUNCTION()
	void OnMeleeHit(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	/** 현재 재생 중인 공격 섹션 인덱스 */
	int32 ComboIndex = 0;

	/** 콤보 입력 구간이 열려 있는지 */
	bool bComboWindowOpen = false;

	/** 다음 공격 입력이 예약되었는지 */
	bool bComboQueued = false;
};
