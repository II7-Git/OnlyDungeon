// OnlyDungeon 보스전 서브시스템 (보스전 전용 규칙)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ODBossEncounterSubsystem.generated.h"

class AODBossEntrance;
class AODCharacterBase;
class AODEnemyCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODBossEncounterStartedSignature, AODEnemyCharacter*, Boss);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODBossEncounterEndedSignature, bool, bVictory);

/**
 * 보스전의 진행 상태를 관리한다. (게임 흐름 분담: 공통 규칙 = AODGameMode, 보스전 = 이 클래스, 필드 = UODFieldSubsystem)
 * - 보스방 입구가 입장이 끝나면 StartEncounter를 호출한다. 대기 중이던 보스가 전투를 시작한다.
 * - 보스가 모두 죽으면 승리로 끝난다. 플레이어가 죽어 부활하면 게임 모드가 패배로 끝낸다.
 * - 입구에서 시작한 보스전에서 처치한 보스는 기억해 두고, 부활·휴식으로 되살리지 않는다. (테스트 전체 초기화만 잊는다)
 * - HUD(보스 체력 바), 입구(승리 후 문 열기)는 게임 모드를 거치지 않고 이 서브시스템의 알림에 직접 연결한다.
 * 이후 페이즈 연출, 승리 보상 같은 보스전 전용 규칙은 여기에 추가한다.
 */
UCLASS()
class ONLYDUNGEON_API UODBossEncounterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 보스전을 시작한다. 이미 진행 중이면 먼저 패배로 끝낸다. */
	UFUNCTION(BlueprintCallable, Category = "OD|BossEncounter")
	void StartEncounter(const TArray<AODEnemyCharacter*>& InBosses, AODBossEntrance* InEntrance);

	/** 보스전을 끝낸다. 패배면 살아 있는 보스를 다시 대기 상태로 돌린다. (체력·위치 초기화는 호출한 쪽이 한다) */
	UFUNCTION(BlueprintCallable, Category = "OD|BossEncounter")
	void EndEncounter(bool bVictory);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|BossEncounter")
	bool IsEncounterActive() const { return bActive; }

	/** 체력 바에 표시할 보스 (살아 있는 첫 번째 보스, 없으면 nullptr) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|BossEncounter")
	AODEnemyCharacter* GetCurrentBoss() const;

	/** 이 보스전을 시작한 입구 */
	AODBossEntrance* GetEntrance() const { return Entrance.Get(); }

	/** 입구에서 시작한 보스전에서 처치한 보스인지. 부활·휴식 초기화에서 이 보스는 되살리지 않는다. */
	bool IsBossDefeated(const AODEnemyCharacter* Boss) const;

	/** [테스트] 처치 기록을 지운다. (F5 전체 초기화, 보스방 문 앞 이동에서 보스를 다시 싸울 수 있게) */
	void ForgetDefeatedBosses();

	UPROPERTY(BlueprintAssignable, Category = "OD|BossEncounter")
	FODBossEncounterStartedSignature OnEncounterStarted;

	UPROPERTY(BlueprintAssignable, Category = "OD|BossEncounter")
	FODBossEncounterEndedSignature OnEncounterEnded;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandleBossDeath(AODCharacterBase* DeadCharacter);

	/** 승리 순간 잠깐 느린 화면을 걸었다가 되돌린다. (게임 시간 배율) */
	void PlayVictorySlowMotion();
	void RestoreTimeDilation();

	TArray<TWeakObjectPtr<AODEnemyCharacter>> Bosses;
	TWeakObjectPtr<AODBossEntrance> Entrance;

	/** 처치한 보스 (레벨의 보스 수만큼이라 몇 개뿐이므로 배열 선형 검색). 저장·로드가 생기면 세이브에 옮긴다. */
	TArray<TWeakObjectPtr<AODEnemyCharacter>> DefeatedBosses;
	bool bActive = false;
	FTimerHandle SlowMotionTimer;
};
