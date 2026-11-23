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
	ResetWorldState();
	RevivePlayerAt(Character, EntryTransform, EODCombatMode::Field);

	UE_LOG(LogOD, Log, TEXT("[테스트] 보스방 문 앞으로 이동 (부활 위치: %s)"), NearestPoint ? *NearestPoint->GetName() : TEXT("변경 없음"));
	return true;
}

void AODGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

void AODGameMode::ResetWorldState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
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
