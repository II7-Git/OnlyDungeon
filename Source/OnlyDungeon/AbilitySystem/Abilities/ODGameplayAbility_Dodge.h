// OnlyDungeon 회피(구르기) 어빌리티

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_Dodge.generated.h"

class UAnimMontage;

/** 구르기 방향 (캐릭터 정면 기준, 시계 방향 45도 단위) */
UENUM(BlueprintType)
enum class EODDodgeDirection : uint8
{
	Front,
	FrontRight,
	Right,
	BackRight,
	Back,
	BackLeft,
	Left,
	FrontLeft,
	MAX UMETA(Hidden)
};

/**
 * 회피 어빌리티.
 * - 락온 아님: 입력 방향으로 즉시 회전한 뒤 정면 구르기. 입력이 없으면 제자리 백스텝(Back 몽타주).
 * - 락온 중: 대상을 바라본 채 입력 방향에 맞는 8방향 구르기.
 * - 무적 구간: 시작 후 InvulnerableStartTime부터 InvulnerableDuration 동안 State.Invulnerable 태그를 붙인다.
 * - 스태미나 소모: CostGameplayEffectClass(GE_Cost_Dodge)로 처리한다. 부족하면 발동하지 않는다.
 * - 상태 태그(State.Dodging)와 차단 태그(Dead/Stunned/Dodging)는 BP(GA_Dodge)에서 지정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_Dodge : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 방향별 구르기 몽타주 (인덱스 = EODDodgeDirection) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Dodge", meta = (ArraySizeEnum = "EODDodgeDirection"))
	TObjectPtr<UAnimMontage> DodgeMontages[static_cast<uint8>(EODDodgeDirection::MAX)];

	/** 몽타주 재생 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Dodge", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	/** 발동 후 무적이 시작되는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Dodge|Invulnerable", meta = (ClampMin = "0.0"))
	float InvulnerableStartTime = 0.05f;

	/** 무적 지속 시간(초). 0이면 무적 없음 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Dodge|Invulnerable", meta = (ClampMin = "0.0"))
	float InvulnerableDuration = 0.4f;

	/** 이 길이보다 짧은 이동 입력은 입력 없음으로 본다 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Dodge", meta = (ClampMin = "0.0"))
	float InputDeadZone = 0.1f;

private:
	/** 정면 기준 로컬 각도로 8방향 중 하나를 고른다 */
	static EODDodgeDirection SelectDirection(const FVector& Forward, const FVector& InputDirection);

	void BeginInvulnerable();
	void EndInvulnerable();

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();

	FTimerHandle InvulnerableStartHandle;
	FTimerHandle InvulnerableEndHandle;
	bool bInvulnerableApplied = false;
};
