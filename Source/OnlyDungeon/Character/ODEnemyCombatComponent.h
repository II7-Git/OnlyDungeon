// OnlyDungeon 적 전투 컴포넌트 (패턴 선택·쿨타임·공격 템포)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ODEnemyCombatComponent.generated.h"

class AODEnemyCharacter;
class UODCombatPatternSet;
struct FODCombatPattern;

/**
 * 적이 어떤 공격을 언제 쓸지 정한다. (StateTree 노드가 이 컴포넌트에 묻는다)
 * - 패턴 선택: 지금 거리·페이즈 태그·패턴 쿨타임에 맞는 패턴 중 가중치로 하나를 고른다.
 * - 공격 템포: 공격이 끝나면 공통 간격(PatternSet::AttackInterval) 동안 다음 공격을 시작하지 않는다. 그 사이 StateTree는 간 보기를 한다.
 * - 빈틈 공략: 대상이 빈틈 상태(물약 마시기·후딜·경직)에 들어설 때마다 한 번 확률을 굴려 공략할지 정한다.
 * - 공격 순번(일반 몬스터): 필드 서브시스템의 순번을 얻은 몬스터만 공격을 시작한다. 공격이 끝나면 돌려준다.
 * 상태는 모두 월드 시간 기준이라 Tick이 필요 없다.
 */
UCLASS(ClassGroup = (OnlyDungeon), meta = (BlueprintSpawnableComponent))
class ONLYDUNGEON_API UODEnemyCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UODEnemyCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UODCombatPatternSet* GetPatternSet() const { return PatternSet; }

	/** 공통 간격이 지났고, (순번을 쓰면) 순번을 얻을 수 있는지 */
	bool IsAttackReady() const;

	/**
	 * 쓸 패턴을 고른다. 없으면 INDEX_NONE.
	 * @param Distance       대상까지 수평 거리
	 * @param bPunish        빈틈 공략 패턴(bPunish)만 고른다.
	 * @param ApproachSlack  사거리(MaxRange)보다 이만큼 멀어도 "걸어가서 쓸" 후보로 친다.
	 */
	int32 SelectPattern(float Distance, bool bPunish, float ApproachSlack) const;

	const FODCombatPattern* GetPattern(int32 Index) const;

	/** 패턴 시작: 쿨타임 시작, 공격 순번 확보 */
	void NotifyPatternStarted(int32 Index);

	/** 패턴 끝 (성공·실패·끊김 모두): 다음 공격까지 공통 간격 시작, 순번 반납 */
	void NotifyPatternFinished(int32 Index);

	/** 대상이 지금 빈틈인지 (PatternSet::OpeningTags) */
	bool IsTargetInOpening(const AActor* Target) const;

	/**
	 * 지금 빈틈을 공략할지. 빈틈이 새로 생길 때마다 PunishChance로 한 번 굴리고, 공략할 패턴이 있어야 참.
	 * StateTree 조건에서 여러 번 불려도 같은 빈틈에서는 결과가 바뀌지 않는다.
	 */
	bool ShouldPunish(const AActor* Target, float Distance, float ApproachSlack) const;

	/** 사망·부활·귀환 때 쿨타임·간격·순번을 처음 상태로 */
	void ResetCombatState();

	/** 소유 적의 상태 태그(페이즈 등)가 바뀐 뒤 호출. 조건이 새로 갖춰진 bUseOnceWhenTagsGained 패턴을 바로 쓰도록 걸어 둔다. */
	void HandleOwnerTagsChanged();

	/** 공격 순번을 갖고 있는지 */
	bool IsHoldingAttackToken() const { return bHoldingToken; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 공격 패턴 데이터. 비우면 패턴 공격을 하지 않는다. (허수아비 등) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Combat")
	TObjectPtr<UODCombatPatternSet> PatternSet;

private:
	AODEnemyCharacter* GetEnemy() const;
	double GetNow() const;
	bool IsPatternUsable(const FODCombatPattern& Pattern, int32 Index, float Distance, bool bPunish, float ApproachSlack) const;
	void ReleaseToken();
	bool HasRequiredTags(const FODCombatPattern& Pattern) const;

	/** 다음 공격으로 바로 쓸 패턴 (bUseOnceWhenTagsGained). 패턴 수가 적어 인덱스별 배열로 둔다. */
	TArray<bool> ForcedPending;

	/** 패턴별로 RequiredTags가 지난번에 갖춰져 있었는지 (새로 갖춰진 순간을 찾기 위해) */
	TArray<bool> RequiredTagsMet;

	/** 패턴별로 다시 쓸 수 있게 되는 월드 시각 */
	TArray<double> PatternReadyTime;

	/** 다음 공격을 시작할 수 있는 월드 시각 */
	double NextAttackTime = 0.0;

	bool bHoldingToken = false;

	/** 빈틈 판정 (조건 평가는 const라 mutable) */
	mutable bool bWasInOpening = false;
	mutable bool bPunishThisOpening = false;
};
