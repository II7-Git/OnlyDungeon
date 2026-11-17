// OnlyDungeon 적 공격 패턴 데이터 (보스·일반 몬스터 공용)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ODCombatPatternSet.generated.h"

class UGameplayAbility;

/** 공격 패턴 하나 */
USTRUCT(BlueprintType)
struct FODCombatPattern
{
	GENERATED_BODY()

	/** 패턴 이름 (로그·디버그 표시용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern")
	FName Name;

	/** 발동할 어빌리티. 적의 어빌리티 세트(기본 세트 또는 무기 세트)에 부여되어 있어야 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** 입력 횟수 (첫 발동 포함). 콤보 어빌리티(약공 연타 등)는 2 이상이면 그만큼 이어 친다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern", meta = (ClampMin = "1"))
	int32 PressCount = 1;

	/** 이어 치기 입력 간격 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern", meta = (ClampMin = "0.05", Units = "s"))
	float PressInterval = 0.3f;

	/** 이 거리 이상에서만 쓴다. (돌진처럼 가까우면 어색한 패턴) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern|Range", meta = (ClampMin = "0.0", Units = "cm"))
	float MinRange = 0.0f;

	/** 이 거리 안에서 쓴다. 더 멀면 걸어서 다가간 뒤 쓴다. (접근 허용 거리는 StateTree 태스크가 정함) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern|Range", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxRange = 250.0f;

	/** 쓸 수 있는 패턴 중 이 비율로 고른다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** 이 패턴을 다시 쓰기까지의 시간 (같은 패턴 연속 사용 방지) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float Cooldown = 4.0f;

	/** 적에게 이 태그가 모두 있을 때만 쓴다. (예: State.Phase.2 = 2페이즈 전용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern", meta = (Categories = "State"))
	FGameplayTagContainer RequiredTags;

	/** 빈틈 공략(플레이어가 물약을 마시거나 후딜 중)에 쓸 수 있는 패턴. 보통 빠르게 거리를 좁히는 공격 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern")
	bool bPunish = false;

	/** 빈틈 공략 때만 쓰고 평소에는 고르지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern", meta = (EditCondition = "bPunish"))
	bool bPunishOnly = false;

	/** 이 패턴이 끝난 뒤 공통 공격 간격에 더할 쉼 (큰 공격 뒤 숨 고르기) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern", meta = (ClampMin = "0.0", Units = "s"))
	float ExtraRecovery = 0.0f;
};

/**
 * 적 하나의 공격 패턴 묶음과 공격 템포. (소울라이크 보스·몬스터 AI의 데이터)
 * - 적 전투 컴포넌트(UODEnemyCombatComponent)가 이 데이터로 패턴을 고르고 쿨타임·공통 간격을 관리한다.
 * - StateTree는 "공격 가능? → 패턴 실행" / "빈틈? → 빈틈 공략 패턴" / "그 외 → 간 보기(주위 돌기)"만 정하고, 수치는 모두 여기서 조정한다.
 */
UCLASS(BlueprintType)
class ONLYDUNGEON_API UODCombatPatternSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 공격 패턴 목록 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Pattern", meta = (TitleProperty = "Name"))
	TArray<FODCombatPattern> Patterns;

	/** 공격이 끝난 뒤 다음 공격까지의 최소 간격 (최소~최대 사이 무작위). 너무 잦은 공격을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Tempo")
	FVector2D AttackInterval = FVector2D(1.2, 2.2);

	/** 적에게 이 태그가 있으면 공격 간격에 AggressiveIntervalScale을 곱한다. (2페이즈에서 더 공격적으로) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Tempo", meta = (Categories = "State"))
	FGameplayTag AggressiveTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Tempo", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float AggressiveIntervalScale = 0.6f;

	/** 간 보기: 대상과 유지하려는 거리 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Strafe", meta = (ClampMin = "50.0", Units = "cm"))
	float StrafeDistance = 450.0f;

	/** 간 보기: 한 번에 도는 시간 (최소~최대 사이 무작위) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Strafe")
	FVector2D StrafeDuration = FVector2D(1.5, 3.0);

	/** 간 보기 이동 속도 (걷기) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Strafe", meta = (ClampMin = "10.0", Units = "cm/s"))
	float StrafeSpeed = 180.0f;

	/** 빈틈을 발견했을 때 공략할 확률 (빈틈마다 한 번 굴린다. 매번 공략하면 너무 기계적이다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Punish", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PunishChance = 0.85f;

	/** 빈틈으로 보는 대상 상태 (물약 마시기, 공격·회피 후딜(캔슬 구간), 경직) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Punish", meta = (Categories = "State"))
	FGameplayTagContainer OpeningTags;

	/** 일반 몬스터: 플레이어 한 명을 동시에 공격하는 수를 제한하는 공격 순번을 쓴다. (보스는 끔) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Group")
	bool bUseAttackToken = false;
};
