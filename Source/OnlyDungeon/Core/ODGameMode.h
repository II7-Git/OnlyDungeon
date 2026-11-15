// OnlyDungeon 게임 모드

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Character/ODCombatModeComponent.h"
#include "ODGameMode.generated.h"

class AODCharacterBase;
class AODRespawnPoint;

/**
 * OnlyDungeon 기본 게임 모드. (BP_ODGameMode의 부모)
 * - 레벨이 어떤 전투 모드로 시작하는지 정한다. 레벨별로 다르게 하려면 월드 세팅에서 이 클래스를 상속한 게임 모드를 지정한다.
 * - 필드·보스전 공통 규칙을 담당한다: 플레이어 사망 → 자동 부활, 리스폰 포인트 휴식.
 *   (보스전 전용 = UODBossEncounterSubsystem, 필드 적 관리 = UODFieldSubsystem)
 * - 부활하면 잡몹과 보스가 모두 초기화된다. 주운 아이템·재화는 유지한다. (기획서 2장, 아이템은 이후 단계)
 */
UCLASS()
class ONLYDUNGEON_API AODGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	EODCombatMode GetInitialCombatMode() const { return InitialCombatMode; }

	/** 리스폰 포인트에서 휴식: 부활 위치 등록, 플레이어 회복, 적 전체 초기화 */
	void RestAtRespawnPoint(AODRespawnPoint* Point, APawn* Pawn);

	/** 마지막으로 휴식한 리스폰 포인트 (없으면 nullptr → 시작 위치에서 부활) */
	AODRespawnPoint* GetActiveRespawnPoint() const { return ActiveRespawnPoint.Get(); }

	/** 보스전·적·보스방 입구를 처음 상태로 되돌린다. (부활, 테스트 초기화) */
	void ResetWorldState();

	/** 예약된 부활을 취소한다. (테스트 초기화로 이미 되살린 경우) */
	void CancelPendingRespawn();

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 레벨 시작 시 플레이어의 전투 모드 (필드 = 쿼터뷰, 보스 = 3인칭) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Mode")
	EODCombatMode InitialCombatMode = EODCombatMode::Field;

	/** 사망 후 부활까지 걸리는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Respawn", meta = (ClampMin = "0.5"))
	float RespawnDelay = 3.0f;

	/** 부활 직전 화면이 어두워지는 시간(초). 부활 후 같은 시간 동안 다시 밝아진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Respawn", meta = (ClampMin = "0.0"))
	float FadeTime = 1.0f;

private:
	UFUNCTION()
	void HandlePlayerDeath(AODCharacterBase* DeadCharacter);

	void BeginRespawnFade();
	void RespawnPlayer();

	/** 플레이어를 부활시키고 지정 위치·모드로 옮긴다. */
	void RevivePlayerAt(AODCharacterBase* Player, const FTransform& Transform, EODCombatMode Mode) const;

	TWeakObjectPtr<AODCharacterBase> Player;
	TWeakObjectPtr<AODRespawnPoint> ActiveRespawnPoint;
	FTransform PlayerStartTransform = FTransform::Identity;

	FTimerHandle FadeTimer;
	FTimerHandle RespawnTimer;
};
