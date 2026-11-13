// OnlyDungeon 근접 콤보 공격 어빌리티 (약공격 등)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_MeleeCombo.generated.h"

class UAnimMontage;

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
 * - 입력이 없으면 후딜 섹션까지 재생되고 종료된다. 스태미나가 부족하면 다음 타격으로 넘어가지 않는다.
 * - 상태 태그(State.Attacking)와 차단 태그는 BP(GA_*)에서 지정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_MeleeCombo : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_MeleeCombo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** 콤보 몽타주 (섹션으로 나뉜 하나의 몽타주) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo")
	TObjectPtr<UAnimMontage> ComboMontage;

	/** 공격 섹션 이름 (콤보 순서대로) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Combo")
	TArray<FName> ComboSections;

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

	UFUNCTION()
	void OnComboWindowEvent(FGameplayEventData Payload);

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
