// OnlyDungeon 방어 어빌리티 (패링 / 가드)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_Defense.generated.h"

class UAnimMontage;

/** 방어 방식 (무기마다 다름. 기획: 한손검 = 패링, 대검 = 가드) */
UENUM(BlueprintType)
enum class EODDefenseType : uint8
{
	/** 짧은 판정 구간 동안 정면 공격을 받으면 피해 없이 공격자를 경직시킨다. 누르면 1회 발동 */
	Parry,

	/** 누르고 있는 동안 정면 공격 피해를 줄이고 스태미나로 막는다. 스태미나가 바닥나면 가드가 깨진다 */
	Guard
};

/**
 * 방어 어빌리티. 무기 어빌리티 세트에 들어가 방어 키(InputTag.Defense)로 발동한다.
 * - 패링: 몽타주 재생 + ParryWindowStart부터 ParryWindowDuration 동안 State.Parrying.
 *         실제 패링 판정·공격자 경직은 피해 계산(UODDamageExecution)이 처리한다.
 * - 가드: 루프 몽타주 재생. State.Defending(ActivationOwnedTags, BP 지정) 동안 피해 계산이 감소·스태미나 소모를 처리한다.
 *         발동 방식은 BP에서 WhileInputActive로 지정해 키를 떼면 끝나게 한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_Defense : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_Defense(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 방어 방식 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Defense")
	EODDefenseType DefenseType = EODDefenseType::Parry;

	/** 방어 몽타주 (패링: 1회 재생 / 가드: 스스로 반복되는 섹션을 가진 몽타주) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Defense")
	TObjectPtr<UAnimMontage> DefenseMontage;

	/** 발동 시 입력 방향으로 회전할지 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Defense")
	bool bRotateToInput = true;

	/** [패링] 발동 후 판정이 시작되는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Defense|Parry", meta = (ClampMin = "0.0", EditCondition = "DefenseType == EODDefenseType::Parry"))
	float ParryWindowStart = 0.03f;

	/** [패링] 판정 지속 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Defense|Parry", meta = (ClampMin = "0.0", EditCondition = "DefenseType == EODDefenseType::Parry"))
	float ParryWindowDuration = 0.25f;

private:
	void BeginParryWindow();
	void EndParryWindow();

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	FTimerHandle ParryStartHandle;
	FTimerHandle ParryEndHandle;
	bool bParryWindowApplied = false;
};
