// OnlyDungeon 게임 모드

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Character/ODCombatModeComponent.h"
#include "ODGameMode.generated.h"

class AODCharacterBase;
class AODRespawnPoint;
class UODAudioSet;
class UODSaveGame;
class APlayerCameraManager;

/** 게임 흐름 상태. 타이틀·클리어 화면 동안에는 게임이 멈춘다. */
UENUM(BlueprintType)
enum class EODGameFlowState : uint8
{
	/** 타이틀 화면 (게임 멈춤) */
	Title,
	/** 플레이 중 */
	Playing,
	/** 보스 처치 후 클리어 화면 (게임 멈춤) */
	Cleared
};

/** 클리어 화면에 보여 줄 기록 */
USTRUCT(BlueprintType)
struct FODRunStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OD|Flow")
	int32 DeathCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OD|Flow")
	double PlayTimeSeconds = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODGameFlowChangedSignature, EODGameFlowState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODMenuPauseChangedSignature, bool, bPaused);

/**
 * OnlyDungeon 기본 게임 모드. (BP_ODGameMode의 부모)
 * - 레벨이 어떤 전투 모드로 시작하는지 정한다. 레벨별로 다르게 하려면 월드 세팅에서 이 클래스를 상속한 게임 모드를 지정한다.
 * - 필드·보스전 공통 규칙을 담당한다: 플레이어 사망 → 자동 부활, 리스폰 포인트 휴식.
 *   (보스전 전용 = UODBossEncounterSubsystem, 필드 적 관리 = UODFieldSubsystem)
 * - 부활하면 잡몹과 보스가 모두 초기화된다. 주운 아이템·재화는 유지한다. (기획서 2장, 아이템은 이후 단계)
 * - 게임 흐름(타이틀 → 플레이 → 클리어)과 일시정지를 관리하고, 진행 상황을 저장 서브시스템에 저장한다.
 *   화면 전환은 HUD가 OnGameFlowChanged·OnMenuPauseChanged를 구독해 처리한다. (게임 모드는 UI를 모른다)
 */
UCLASS()
class ONLYDUNGEON_API AODGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	EODCombatMode GetInitialCombatMode() const { return InitialCombatMode; }

	EODGameFlowState GetFlowState() const { return FlowState; }
	bool IsMenuPaused() const { return bMenuPaused; }

	/** 새 게임: 진행 저장을 지우고 처음 상태로 시작한다. */
	void StartNewGame();

	/** 이어하기: 진행 저장을 불러와 적용한다. 저장이 없거나 쓸 수 없으면 새 게임으로 시작한다. */
	void ContinueGame();

	/** 클리어 화면에서 계속 탐험한다. */
	void ContinueAfterClear();

	/** 진행을 저장하고 레벨을 다시 열어 타이틀로 돌아간다. */
	void ReturnToTitle();

	/** 진행을 저장하고 게임을 끝낸다. */
	void QuitGame();

	/** 일시정지 메뉴를 켜고 끈다. (플레이 중에만) */
	void RequestTogglePause();
	void SetMenuPaused(bool bPaused);

	/** 지금까지의 기록 (사망 횟수, 플레이 시간) */
	FODRunStats GetRunStats() const;

	/** 현재 진행 상황(처치한 보스, 휴식 지점, 무기, 기록)을 저장한다. 타이틀 상태거나 저장 데이터가 없으면 무시한다. */
	void SaveProgress(bool bSync = false);

	UPROPERTY(BlueprintAssignable, Category = "OD|Flow")
	FODGameFlowChangedSignature OnGameFlowChanged;

	UPROPERTY(BlueprintAssignable, Category = "OD|Flow")
	FODMenuPauseChangedSignature OnMenuPauseChanged;

	/** 이 레벨의 배경음·환경음 세트 (UODAudioDirectorSubsystem이 읽는다) */
	const TSoftObjectPtr<UODAudioSet>& GetAudioSet() const { return AudioSet; }

	/**
	 * 리스폰 포인트에서 휴식: 부활 위치 등록, 플레이어 회복, 적 전체 초기화.
	 * 화면을 어둡게 했다가(RestFadeOutTime) 검은 화면에서 회복·적 초기화를 하고(RestBlackHoldTime) 다시 밝힌다(RestFadeInTime).
	 * 적이 제자리로 돌아가는 모습이 보이지 않고, 휴식했다는 구분이 생긴다. 페이드 중 다시 불려도 한 번만 처리한다.
	 */
	void RestAtRespawnPoint(AODRespawnPoint* Point, APawn* Pawn);

	/** 마지막으로 휴식한 리스폰 포인트 (없으면 nullptr → 시작 위치에서 부활) */
	AODRespawnPoint* GetActiveRespawnPoint() const { return ActiveRespawnPoint.Get(); }

	/**
	 * 보스전·적·보스방 입구를 처음 상태로 되돌린다. (부활, 테스트 초기화)
	 * @param bForgetDefeatedBosses true면 보스 처치 기록까지 지워 처치한 보스도 되살린다. (F5·보스방 문 앞 이동 같은 테스트용)
	 */
	void ResetWorldState(bool bForgetDefeatedBosses = false);

	/** 예약된 부활을 취소한다. (테스트 초기화로 이미 되살린 경우) */
	void CancelPendingRespawn();

	/**
	 * [테스트] 플레이어를 보스방 문 앞(입장 시작 지점)으로 옮긴다. F6, 콘솔 명령 OD.Test.GoToBossDoor,
	 * PIE 시작 옵션 OD.Test.StartAtBossDoor 1에서 쓴다. 부활과 같은 경로로 월드를 초기화하고(보스전·입구·적),
	 * 문 근처의 리스폰 포인트를 부활 위치로 등록해 보스전에서 죽어도 문 앞에서 다시 시작한다.
	 * @return 보스방 입구가 있어 옮겼으면 true
	 */
	bool TeleportPlayerToBossDoor();

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 레벨을 열면 타이틀 화면부터 보여 준다. (테스트 맵용 게임 모드는 끈다, 콘솔 OD.Test.SkipTitle 1로도 건너뜀) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Flow")
	bool bShowTitleOnStart = true;

	/** 보스 처치 후 클리어 화면이 뜨기까지의 시간(초). 승리 연출(느린 화면·문구)이 끝난 뒤가 되도록 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Flow", meta = (ClampMin = "0.0", Units = "s"))
	float ClearScreenDelay = 4.5f;

	/** 레벨 시작 시 플레이어의 전투 모드 (필드 = 쿼터뷰, 보스 = 3인칭) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Mode")
	EODCombatMode InitialCombatMode = EODCombatMode::Field;

	/** 배경음·환경음 세트. 비어 있으면 배경음 없이 진행한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Audio")
	TSoftObjectPtr<UODAudioSet> AudioSet;

	/** 사망 후 부활까지 걸리는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Respawn", meta = (ClampMin = "0.5"))
	float RespawnDelay = 3.0f;

	/** 부활 직전 화면이 어두워지는 시간(초). 부활 후 같은 시간 동안 다시 밝아진다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Respawn", meta = (ClampMin = "0.0"))
	float FadeTime = 1.0f;

	/** [테스트] 보스방 문 앞 이동 시, 입장 지점에서 이 거리 안의 리스폰 포인트를 부활 위치로 등록한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Test", meta = (ClampMin = "0.0", Units = "cm"))
	float BossDoorRespawnSearchRadius = 2000.0f;

	/** 휴식: 화면이 검게 변하는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Rest", meta = (ClampMin = "0.0", Units = "s"))
	float RestFadeOutTime = 0.6f;

	/** 휴식: 검은 화면을 유지하는 시간(초). 이 사이에 회복·적 초기화를 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Rest", meta = (ClampMin = "0.0", Units = "s"))
	float RestBlackHoldTime = 0.5f;

	/** 휴식: 화면이 다시 밝아지는 시간(초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Rest", meta = (ClampMin = "0.0", Units = "s"))
	float RestFadeInTime = 0.8f;

private:
	UFUNCTION()
	void HandlePlayerDeath(AODCharacterBase* DeadCharacter);

	/** 보스전 종료 알림. 입구가 있는 보스전 승리면 저장하고(첫 클리어면) 클리어 화면을 예약한다. */
	UFUNCTION()
	void HandleBossEncounterEnded(bool bVictory);

	void ShowClearScreen();
	void SetFlowState(EODGameFlowState NewState);

	/** 불러온 저장을 월드에 적용한다. (처치한 보스, 휴식 지점, 무기, 기록) */
	void ApplyLoadedProgress(const UODSaveGame* Save);

	/** 검은 화면에서 회복·적 초기화 */
	void ApplyPendingRest();
	void EndRestFade();
	/** 진행 중인 휴식 페이드를 멈춘다. (사망, 테스트 초기화, 레벨 종료) */
	void CancelRestFade();
	APlayerCameraManager* GetPlayerCameraManager() const;

	void BeginRespawnFade();
	void RespawnPlayer();

	/** 플레이어를 부활시키고 지정 위치·모드로 옮긴다. */
	void RevivePlayerAt(AODCharacterBase* Player, const FTransform& Transform, EODCombatMode Mode) const;

	TWeakObjectPtr<AODCharacterBase> Player;
	TWeakObjectPtr<AODRespawnPoint> ActiveRespawnPoint;
	FTransform PlayerStartTransform = FTransform::Identity;

	FTimerHandle FadeTimer;
	FTimerHandle RespawnTimer;

	/** 휴식 페이드 진행 중 (중복 휴식 방지) */
	bool bRestFadeInProgress = false;
	TWeakObjectPtr<AODCharacterBase> PendingRestCharacter;
	FTimerHandle RestApplyTimer;
	FTimerHandle RestFadeInTimer;

	EODGameFlowState FlowState = EODGameFlowState::Playing;
	bool bMenuPaused = false;
	bool bGameCleared = false;
	int32 DeathCount = 0;

	/** 이전 세션까지의 플레이 시간 + 이번 세션 시작 시각(게임 시간: 일시정지 동안 멈춘다) */
	double PlayTimeBase = 0.0;
	double PlayTimeSessionStart = 0.0;

	FTimerHandle ClearScreenTimer;
};
