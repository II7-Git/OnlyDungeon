// OnlyDungeon 게임 모드

#include "ODGameMode.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Character/ODCharacterBase.h"
#include "Character/ODCombatModeComponent.h"
#include "Core/ODAudioDirectorSubsystem.h"
#include "Core/ODSaveGame.h"
#include "Core/ODSaveSubsystem.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponData.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Character/ODEnemyCharacter.h"
#include "Core/ODBossEncounterSubsystem.h"
#include "Core/ODFieldSubsystem.h"
#include "Core/ODLogChannels.h"
#include "World/ODBossEntrance.h"
#include "World/ODRespawnPoint.h"

namespace ODGameModeTest
{
	/** [테스트] 켜면 PIE·게임을 보스방 문 앞에서 시작한다. (에디터 콘솔에서 OD.Test.StartAtBossDoor 1) */
	static TAutoConsoleVariable<bool> CVarStartAtBossDoor(
		TEXT("OD.Test.StartAtBossDoor"),
		false,
		TEXT("[테스트] 1이면 플레이어를 보스방 문 앞에서 시작한다."));

	/** [테스트] 1이면 타이틀 화면 없이 바로 플레이한다. (진행 저장을 건드리지 않음) PIE 자동 테스트용 */
	static TAutoConsoleVariable<bool> CVarSkipTitle(
		TEXT("OD.Test.SkipTitle"),
		false,
		TEXT("[테스트] 1이면 타이틀 없이 바로 시작한다. (진행 저장을 건드리지 않음)"));

	/** [테스트] 콘솔에서 보스방 문 앞으로 이동 */
	static FAutoConsoleCommandWithWorld GoToBossDoorCommand(
		TEXT("OD.Test.GoToBossDoor"),
		TEXT("[테스트] 플레이어를 보스방 문 앞으로 옮긴다. (F6과 같음)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (AODGameMode* GameMode = World ? World->GetAuthGameMode<AODGameMode>() : nullptr)
			{
				GameMode->TeleportPlayerToBossDoor();
			}
		}));
}

void AODGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// 플레이어 사망을 감시하고, 휴식 전 사망에 대비해 시작 위치를 기억한다.
	if (AODCharacterBase* Character = IsValid(NewPlayer) ? Cast<AODCharacterBase>(NewPlayer->GetPawn()) : nullptr)
	{
		Player = Character;
		PlayerStartTransform = FTransform(FRotator(0.0f, Character->GetActorRotation().Yaw, 0.0f), Character->GetActorLocation());
		Character->OnDeath.AddUniqueDynamic(this, &ThisClass::HandlePlayerDeath);

		// [테스트] 보스방 문 앞에서 시작: 시작 모드 적용이 끝난 다음 틱에 옮긴다.
		if (ODGameModeTest::CVarStartAtBossDoor.GetValueOnGameThread())
		{
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				TeleportPlayerToBossDoor();
			}));
		}
	}
}

bool AODGameMode::TeleportPlayerToBossDoor()
{
	UWorld* World = GetWorld();
	AODCharacterBase* Character = Player.Get();
	if (!World || !IsValid(Character))
	{
		return false;
	}

	// 테스트 전용 한 번 호출이라 액터 순회를 쓴다. (매 프레임 경로 아님)
	const AODBossEntrance* Entrance = nullptr;
	for (TActorIterator<AODBossEntrance> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Entrance = *It;
			break;
		}
	}
	if (!Entrance)
	{
		UE_LOG(LogOD, Warning, TEXT("[테스트] 보스방 입구가 없어 이동하지 못했습니다."));
		return false;
	}
	const FTransform EntryTransform = Entrance->GetEntryTransform();

	// 문 근처 리스폰 포인트를 부활 위치로 등록한다. (보스전에서 죽으면 문 앞에서 다시 시작)
	AODRespawnPoint* NearestPoint = nullptr;
	double NearestDistSq = FMath::Square(static_cast<double>(BossDoorRespawnSearchRadius));
	for (TActorIterator<AODRespawnPoint> It(World); It; ++It)
	{
		const double DistSq = FVector::DistSquared(It->GetActorLocation(), EntryTransform.GetLocation());
		if (IsValid(*It) && DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			NearestPoint = *It;
		}
	}
	if (NearestPoint)
	{
		ActiveRespawnPoint = NearestPoint;
	}

	// 부활과 같은 경로: 예약된 부활·휴식 취소 → 보스전·입구·적 초기화 → 문 앞에 필드 모드로 세운다.
	CancelPendingRespawn();
	ResetWorldState(true);
	RevivePlayerAt(Character, EntryTransform, EODCombatMode::Field);

	UE_LOG(LogOD, Log, TEXT("[테스트] 보스방 문 앞으로 이동 (부활 위치: %s)"), NearestPoint ? *NearestPoint->GetName() : TEXT("변경 없음"));
	return true;
}

void AODGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr)
	{
		Encounter->OnEncounterEnded.AddUniqueDynamic(this, &ThisClass::HandleBossEncounterEnded);
	}

	if (bShowTitleOnStart && !ODGameModeTest::CVarSkipTitle.GetValueOnGameThread())
	{
		SetFlowState(EODGameFlowState::Title);
	}
	else
	{
		PlayTimeSessionStart = GetWorld()->GetTimeSeconds();
		SetFlowState(EODGameFlowState::Playing);
	}
}

void AODGameMode::SetFlowState(EODGameFlowState NewState)
{
	FlowState = NewState;
	bMenuPaused = false;
	// 타이틀·클리어 화면 동안에는 게임(이동·AI·타이머·플레이 시간)이 멈춘다.
	UGameplayStatics::SetGamePaused(this, NewState != EODGameFlowState::Playing);
	OnGameFlowChanged.Broadcast(NewState);
}

void AODGameMode::StartNewGame()
{
	if (FlowState != EODGameFlowState::Title)
	{
		return;
	}
	if (UODSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UODSaveSubsystem>() : nullptr)
	{
		Save->CreateNewProgress();
	}
	DeathCount = 0;
	PlayTimeBase = 0.0;
	bGameCleared = false;
	ActiveRespawnPoint.Reset();
	ResetWorldState(true);

	PlayTimeSessionStart = GetWorld()->GetTimeSeconds();
	SetFlowState(EODGameFlowState::Playing);
	SaveProgress();
	UE_LOG(LogOD, Log, TEXT("새 게임 시작"));
}

void AODGameMode::ContinueGame()
{
	if (FlowState != EODGameFlowState::Title)
	{
		return;
	}
	UODSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UODSaveSubsystem>() : nullptr;
	const UODSaveGame* Loaded = Save ? Save->LoadProgress() : nullptr;
	if (!Loaded)
	{
		StartNewGame();
		return;
	}
	ApplyLoadedProgress(Loaded);
	PlayTimeSessionStart = GetWorld()->GetTimeSeconds();
	SetFlowState(EODGameFlowState::Playing);
	UE_LOG(LogOD, Log, TEXT("이어하기: 처치한 보스 %d, 휴식 지점 %s"), Loaded->DefeatedBossNames.Num(), *Loaded->LastRespawnPointName.ToString());
}

void AODGameMode::ApplyLoadedProgress(const UODSaveGame* Save)
{
	UWorld* World = GetWorld();
	if (!World || !Save)
	{
		return;
	}
	DeathCount = Save->DeathCount;
	PlayTimeBase = Save->PlayTimeSeconds;
	bGameCleared = Save->bGameCleared;

	// 1) 처치한 보스: 레벨의 같은 이름 보스를 처치로 등록한다. (레벨에서 사라진 보스는 건너뜀)
	UODBossEncounterSubsystem* Encounter = World->GetSubsystem<UODBossEncounterSubsystem>();
	const UODFieldSubsystem* Field = World->GetSubsystem<UODFieldSubsystem>();
	if (Encounter && Field)
	{
		Field->ForEachEnemy([Encounter, Save](AODEnemyCharacter& Enemy)
		{
			if (Save->DefeatedBossNames.Contains(Enemy.GetFName()))
			{
				Encounter->MarkDefeatedFromSave(&Enemy);
			}
		});
	}

	// 2) 입구(처치했으면 열린 채)·적을 그 상태에 맞게 맞춘다.
	ResetWorldState(false);

	// 3) 마지막 휴식 지점에서 저장된 무기로 시작한다. (불러올 때 한 번만 하는 검색이라 액터 순회를 쓴다)
	AODRespawnPoint* Point = nullptr;
	if (!Save->LastRespawnPointName.IsNone())
	{
		for (TActorIterator<AODRespawnPoint> It(World); It; ++It)
		{
			if (It->GetFName() == Save->LastRespawnPointName)
			{
				Point = *It;
				break;
			}
		}
	}
	ActiveRespawnPoint = Point;

	AODCharacterBase* Character = Player.Get();
	if (!IsValid(Character))
	{
		return;
	}
	if (UODWeaponData* Weapon = Save->EquippedWeapon.LoadSynchronous(); IsValid(Weapon) && IsValid(Character->GetEquipmentComponent()))
	{
		Character->GetEquipmentComponent()->EquipWeapon(Weapon);
	}
	if (IsValid(Point))
	{
		RevivePlayerAt(Character, Point->GetRespawnTransform(), Point->GetRespawnCombatMode());
	}
}

void AODGameMode::SaveProgress(bool bSync)
{
	UODSaveSubsystem* SaveSystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UODSaveSubsystem>() : nullptr;
	UODSaveGame* Save = SaveSystem ? SaveSystem->GetProgress() : nullptr;
	if (FlowState == EODGameFlowState::Title || !Save)
	{
		return;
	}

	Save->DefeatedBossNames.Reset();
	if (const UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr)
	{
		Encounter->GetDefeatedBossNames(Save->DefeatedBossNames);
	}
	const AODRespawnPoint* Point = ActiveRespawnPoint.Get();
	Save->LastRespawnPointName = IsValid(Point) ? Point->GetFName() : NAME_None;
	const AODCharacterBase* Character = Player.Get();
	const UODEquipmentComponent* Equipment = IsValid(Character) ? Character->GetEquipmentComponent() : nullptr;
	Save->EquippedWeapon = IsValid(Equipment) ? Equipment->GetWeaponData() : nullptr;

	const FODRunStats Stats = GetRunStats();
	Save->DeathCount = Stats.DeathCount;
	Save->PlayTimeSeconds = Stats.PlayTimeSeconds;
	Save->bGameCleared = bGameCleared;
	SaveSystem->SaveProgress(bSync);
}

FODRunStats AODGameMode::GetRunStats() const
{
	FODRunStats Stats;
	Stats.DeathCount = DeathCount;
	Stats.PlayTimeSeconds = PlayTimeBase;
	if (FlowState != EODGameFlowState::Title && GetWorld())
	{
		Stats.PlayTimeSeconds += FMath::Max(GetWorld()->GetTimeSeconds() - PlayTimeSessionStart, 0.0);
	}
	return Stats;
}

void AODGameMode::RequestTogglePause()
{
	if (FlowState == EODGameFlowState::Playing)
	{
		SetMenuPaused(!bMenuPaused);
	}
}

void AODGameMode::SetMenuPaused(bool bPaused)
{
	if (FlowState != EODGameFlowState::Playing || bMenuPaused == bPaused)
	{
		return;
	}
	bMenuPaused = bPaused;
	UGameplayStatics::SetGamePaused(this, bPaused);
	OnMenuPauseChanged.Broadcast(bPaused);
}

void AODGameMode::HandleBossEncounterEnded(bool bVictory)
{
	// 입구에서 시작한 보스전의 승리만 클리어다. (필드 정예 처치는 아님) 알림 시점에는 입구가 아직 남아 있다.
	const UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr;
	if (!bVictory || !Encounter || !Encounter->GetEntrance())
	{
		return;
	}
	SaveProgress();
	if (!bGameCleared)
	{
		GetWorldTimerManager().SetTimer(ClearScreenTimer, this, &ThisClass::ShowClearScreen, FMath::Max(ClearScreenDelay, KINDA_SMALL_NUMBER), false);
	}
}

void AODGameMode::ShowClearScreen()
{
	if (FlowState != EODGameFlowState::Playing)
	{
		return;
	}
	bGameCleared = true;
	SaveProgress();
	// 일시정지 메뉴가 열려 있었어도 클리어 화면으로 바꾼다. (SetFlowState가 메뉴 일시정지를 푼다)
	if (bMenuPaused)
	{
		OnMenuPauseChanged.Broadcast(false);
	}
	SetFlowState(EODGameFlowState::Cleared);
	UE_LOG(LogOD, Log, TEXT("던전 클리어: 사망 %d, 플레이 %.0f초"), DeathCount, GetRunStats().PlayTimeSeconds);
}

void AODGameMode::ContinueAfterClear()
{
	if (FlowState == EODGameFlowState::Cleared)
	{
		SetFlowState(EODGameFlowState::Playing);
	}
}

void AODGameMode::ReturnToTitle()
{
	SaveProgress(true);
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void AODGameMode::QuitGame()
{
	SaveProgress(true);
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
}

void AODGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ClearScreenTimer);
	if (UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr)
	{
		Encounter->OnEncounterEnded.RemoveDynamic(this, &ThisClass::HandleBossEncounterEnded);
	}
	GetWorldTimerManager().ClearTimer(FadeTimer);
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	CancelRestFade();
	if (AODCharacterBase* Character = Player.Get())
	{
		Character->OnDeath.RemoveDynamic(this, &ThisClass::HandlePlayerDeath);
	}
	Super::EndPlay(EndPlayReason);
}

void AODGameMode::HandlePlayerDeath(AODCharacterBase* DeadCharacter)
{
	++DeathCount;
	UE_LOG(LogOD, Log, TEXT("플레이어 사망 → %.1f초 뒤 부활"), RespawnDelay);

	// 휴식 페이드 도중 죽으면 휴식을 취소한다. (부활 페이드가 화면을 맡는다)
	CancelRestFade();

	if (UODAudioDirectorSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UODAudioDirectorSubsystem>() : nullptr)
	{
		Audio->NotifyPlayerDied();
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	const float FadeStart = FMath::Max(RespawnDelay - FadeTime, 0.0f);
	if (FadeTime > 0.0f)
	{
		TimerManager.SetTimer(FadeTimer, this, &ThisClass::BeginRespawnFade, FMath::Max(FadeStart, KINDA_SMALL_NUMBER), false);
	}
	TimerManager.SetTimer(RespawnTimer, this, &ThisClass::RespawnPlayer, RespawnDelay, false);
}

void AODGameMode::BeginRespawnFade()
{
	const AODCharacterBase* Character = Player.Get();
	const APlayerController* PC = IsValid(Character) ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (PC && IsValid(PC->PlayerCameraManager))
	{
		PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeTime, FLinearColor::Black, false, true);
	}
}

void AODGameMode::CancelPendingRespawn()
{
	GetWorldTimerManager().ClearTimer(FadeTimer);
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	CancelRestFade();

	const AODCharacterBase* Character = Player.Get();
	const APlayerController* PC = IsValid(Character) ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (PC && IsValid(PC->PlayerCameraManager))
	{
		PC->PlayerCameraManager->StopCameraFade();
	}
}

void AODGameMode::ResetWorldState(bool bForgetDefeatedBosses)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 0) 테스트 전체 초기화: 처치한 보스도 다시 싸울 수 있게 기록을 지운다. (입구·적 초기화보다 먼저)
	if (bForgetDefeatedBosses)
	{
		if (UODBossEncounterSubsystem* Encounter = World->GetSubsystem<UODBossEncounterSubsystem>())
		{
			Encounter->ForgetDefeatedBosses();
		}
	}

	// 1) 보스전 패배 처리 (보스는 대기 상태로)
	if (UODBossEncounterSubsystem* Encounter = World->GetSubsystem<UODBossEncounterSubsystem>())
	{
		Encounter->EndEncounter(false);
	}

	// 2) 보스방 입구: 문을 닫힌 처음 상태로 (들어가 있던 플레이어는 문 앞으로 옮겨진다)
	for (TActorIterator<AODBossEntrance> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			It->ResetForTest();
		}
	}

	// 3) 잡몹과 보스: 시작 위치·체력으로
	if (UODFieldSubsystem* Field = World->GetSubsystem<UODFieldSubsystem>())
	{
		Field->ResetAllEnemies();
	}
}

void AODGameMode::RespawnPlayer()
{
	AODCharacterBase* Character = Player.Get();
	if (!IsValid(Character))
	{
		return;
	}

	ResetWorldState();

	const AODRespawnPoint* Point = ActiveRespawnPoint.Get();
	const FTransform Transform = IsValid(Point) ? Point->GetRespawnTransform() : PlayerStartTransform;
	const EODCombatMode Mode = IsValid(Point) ? Point->GetRespawnCombatMode() : InitialCombatMode;
	RevivePlayerAt(Character, Transform, Mode);

	if (UODAudioDirectorSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UODAudioDirectorSubsystem>() : nullptr)
	{
		Audio->NotifyPlayerRevived();
	}

	if (const APlayerController* PC = Cast<APlayerController>(Character->GetController()); PC && IsValid(PC->PlayerCameraManager))
	{
		PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeTime, FLinearColor::Black, false, false);
	}

	UE_LOG(LogOD, Log, TEXT("플레이어 부활: %s"), IsValid(Point) ? *Point->GetName() : TEXT("시작 위치"));
	SaveProgress();
}

void AODGameMode::RevivePlayerAt(AODCharacterBase* Character, const FTransform& Transform, EODCombatMode Mode) const
{
	Character->Revive();

	// 캡슐 중심 높이만큼 올려서 바닥에 세운다.
	const float HalfHeight = IsValid(Character->GetCapsuleComponent()) ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	Character->TeleportTo(Transform.GetLocation() + FVector(0.0, 0.0, HalfHeight + 2.0), Transform.Rotator());

	if (UODCombatModeComponent* ModeComponent = Character->FindComponentByClass<UODCombatModeComponent>())
	{
		ModeComponent->SetCinematicMode(false);
		ModeComponent->SetCombatMode(Mode, true);
	}
}

void AODGameMode::RestAtRespawnPoint(AODRespawnPoint* Point, APawn* Pawn)
{
	AODCharacterBase* Character = Cast<AODCharacterBase>(Pawn);
	if (!IsValid(Point) || !IsValid(Character) || Character->IsDead())
	{
		return;
	}

	if (bRestFadeInProgress)
	{
		return;
	}

	ActiveRespawnPoint = Point;
	PendingRestCharacter = Character;
	UE_LOG(LogOD, Log, TEXT("휴식: %s (부활 위치 등록)"), *Point->GetName());

	// 화면을 어둡게 한 뒤 검은 화면에서 회복·적 초기화를 한다. 카메라가 없으면 바로 적용한다.
	APlayerCameraManager* CameraManager = GetPlayerCameraManager();
	if (!IsValid(CameraManager) || RestFadeOutTime + RestBlackHoldTime + RestFadeInTime <= 0.0f)
	{
		ApplyPendingRest();
		return;
	}

	bRestFadeInProgress = true;
	CameraManager->StartCameraFade(0.0f, 1.0f, RestFadeOutTime, FLinearColor::Black, false, true);
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.SetTimer(RestApplyTimer, this, &ThisClass::ApplyPendingRest, FMath::Max(RestFadeOutTime, KINDA_SMALL_NUMBER), false);
	TimerManager.SetTimer(RestFadeInTimer, this, &ThisClass::EndRestFade, FMath::Max(RestFadeOutTime + RestBlackHoldTime, KINDA_SMALL_NUMBER), false);
}

void AODGameMode::ApplyPendingRest()
{
	// 휴식: 회복 + 적 초기화. 플레이어는 그 자리에 둔다. (휴식 모션이 끊기지 않도록 Revive 대신 수치만 회복)
	AODCharacterBase* Character = PendingRestCharacter.Get();
	PendingRestCharacter.Reset();
	if (!IsValid(Character) || Character->IsDead())
	{
		return;
	}
	Character->RestoreVitals();
	if (UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr)
	{
		Field->ResetAllEnemies();
	}
	// 휴식 = 저장 지점 (부활 위치 등록과 함께)
	SaveProgress();
}

void AODGameMode::EndRestFade()
{
	bRestFadeInProgress = false;
	if (APlayerCameraManager* CameraManager = GetPlayerCameraManager())
	{
		CameraManager->StartCameraFade(1.0f, 0.0f, RestFadeInTime, FLinearColor::Black, false, false);
	}
}

void AODGameMode::CancelRestFade()
{
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(RestApplyTimer);
	TimerManager.ClearTimer(RestFadeInTimer);
	PendingRestCharacter.Reset();
	if (bRestFadeInProgress)
	{
		bRestFadeInProgress = false;
		if (APlayerCameraManager* CameraManager = GetPlayerCameraManager())
		{
			CameraManager->StopCameraFade();
		}
	}
}

APlayerCameraManager* AODGameMode::GetPlayerCameraManager() const
{
	const AODCharacterBase* Character = Player.Get();
	const APlayerController* PC = IsValid(Character) ? Cast<APlayerController>(Character->GetController()) : nullptr;
	return (PC && IsValid(PC->PlayerCameraManager)) ? PC->PlayerCameraManager.Get() : nullptr;
}
