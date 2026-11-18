// OnlyDungeon 게임 모드

#include "ODGameMode.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Character/ODCharacterBase.h"
#include "Character/ODCombatModeComponent.h"
#include "Core/ODBossEncounterSubsystem.h"
#include "Core/ODFieldSubsystem.h"
#include "Core/ODLogChannels.h"
#include "World/ODBossEntrance.h"
#include "World/ODRespawnPoint.h"

void AODGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// 플레이어 사망을 감시하고, 휴식 전 사망에 대비해 시작 위치를 기억한다.
	if (AODCharacterBase* Character = IsValid(NewPlayer) ? Cast<AODCharacterBase>(NewPlayer->GetPawn()) : nullptr)
	{
		Player = Character;
		PlayerStartTransform = FTransform(FRotator(0.0f, Character->GetActorRotation().Yaw, 0.0f), Character->GetActorLocation());
		Character->OnDeath.AddUniqueDynamic(this, &ThisClass::HandlePlayerDeath);
	}
}

void AODGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FadeTimer);
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	if (AODCharacterBase* Character = Player.Get())
	{
		Character->OnDeath.RemoveDynamic(this, &ThisClass::HandlePlayerDeath);
	}
	Super::EndPlay(EndPlayReason);
}

void AODGameMode::HandlePlayerDeath(AODCharacterBase* DeadCharacter)
{
	UE_LOG(LogOD, Log, TEXT("플레이어 사망 → %.1f초 뒤 부활"), RespawnDelay);

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

	ActiveRespawnPoint = Point;

	// 휴식: 회복 + 적 초기화. 플레이어는 그 자리에 둔다. (휴식 모션이 끊기지 않도록 Revive 대신 수치만 회복)
	Character->RestoreVitals();
	if (UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr)
	{
		Field->ResetAllEnemies();
	}

	UE_LOG(LogOD, Log, TEXT("휴식: %s (부활 위치 등록)"), *Point->GetName());
}
