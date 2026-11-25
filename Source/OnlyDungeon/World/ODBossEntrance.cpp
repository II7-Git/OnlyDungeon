// OnlyDungeon 보스방 입구 (문 + 입장 컷신)

#include "ODBossEntrance.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "TimerManager.h"
#include "Character/ODCombatModeComponent.h"
#include "Character/ODEnemyCharacter.h"
#include "Core/ODBossEncounterSubsystem.h"
#include "Core/ODLogChannels.h"
#include "Interaction/ODInteractionComponent.h"

namespace ODBossEntrance
{
	constexpr float DoorTickInterval = 1.0f / 60.0f;
	/** 문짝·상인방·안개 메시는 엔진 기본 큐브·판(100cm) 기준으로 크기를 맞춘다. */
	constexpr double BasicShapeSize = 100.0;
	/** 상인방 두께, 문 양옆으로 더 덮는 폭 */
	constexpr double LintelDepth = 40.0;
	constexpr double LintelOverhang = 100.0;
	/** 안개판이 문보다 넓고 높게 덮는 여유 */
	constexpr double FogMargin = 60.0;
	/** 상호작용 범위: 문 앞 깊이·높이 반값, 문 폭 밖으로 더 받는 폭 */
	constexpr double TriggerHalfDepth = 200.0;
	constexpr double TriggerHalfHeight = 150.0;
	constexpr double TriggerSideMargin = 100.0;
	constexpr double TriggerMinHalfWidth = 300.0;
	/** 안개 재질의 짙기 파라미터 (1 = 짙음, 0 = 다 걷힘) */
	const FName FogFadeParameter(TEXT("Fade"));
}

AODBossEntrance::AODBossEntrance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 컷신 중 캐릭터를 걷게 할 때만 Tick을 켠다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 문짝: 경첩(회전축)에 붙인다. 메시는 BP에서 지정 (기본 크기는 100cm 큐브 기준)
	LeftHinge = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHinge"));
	LeftHinge->SetupAttachment(SceneRoot);

	LeftDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoor"));
	LeftDoor->SetupAttachment(LeftHinge);

	RightHinge = CreateDefaultSubobject<USceneComponent>(TEXT("RightHinge"));
	RightHinge->SetupAttachment(SceneRoot);

	RightDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoor"));
	RightDoor->SetupAttachment(RightHinge);

	Lintel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lintel"));
	Lintel->SetupAttachment(SceneRoot);

	// 안개벽은 보이기만 하는 판이다. 실제로 막는 것은 문짝이다.
	FogWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FogWall"));
	FogWall->SetupAttachment(SceneRoot);
	FogWall->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	FogWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FogWall->SetCastShadow(false);

	InteractionTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionTrigger"));
	InteractionTrigger->SetupAttachment(SceneRoot);
	InteractionTrigger->SetCollisionProfileName(TEXT("Trigger"));

	EntryPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("EntryPoint"));
	EntryPoint->SetupAttachment(SceneRoot);
	EntryPoint->SetRelativeLocation(FVector(-250.0, 0.0, 0.0));
	EntryPoint->ArrowColor = FColor::Green;

	RoomPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RoomPoint"));
	RoomPoint->SetupAttachment(SceneRoot);
	RoomPoint->SetRelativeLocation(FVector(700.0, 0.0, 0.0));
	RoomPoint->ArrowColor = FColor::Red;

	InteractionText = NSLOCTEXT("OnlyDungeon", "BossEntranceEnter", "보스방 입장");

	ApplyDoorLayout();
}

void AODBossEntrance::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 레벨에서 DoorWidth·DoorHeight를 바꾸면 바로 반영되게 한다.
	ApplyDoorLayout();
}

void AODBossEntrance::ApplyDoorLayout()
{
	using namespace ODBossEntrance;
	const double HalfWidth = DoorWidth * 0.5;

	// 경첩은 문 양끝, 문짝은 경첩에서 문 가운데 쪽으로 반 폭만큼
	LeftHinge->SetRelativeLocation(FVector(0.0, -HalfWidth, 0.0));
	RightHinge->SetRelativeLocation(FVector(0.0, HalfWidth, 0.0));
	if (bFitBasicShapeDoors)
	{
		const FVector LeafScale(DoorThickness / BasicShapeSize, HalfWidth / BasicShapeSize, DoorHeight / BasicShapeSize);
		LeftDoor->SetRelativeLocation(FVector(0.0, HalfWidth * 0.5, DoorHeight * 0.5));
		LeftDoor->SetRelativeScale3D(LeafScale);
		RightDoor->SetRelativeLocation(FVector(0.0, -HalfWidth * 0.5, DoorHeight * 0.5));
		RightDoor->SetRelativeScale3D(LeafScale);

		Lintel->SetRelativeLocation(FVector(0.0, 0.0, DoorHeight + LintelHeight * 0.5));
		Lintel->SetRelativeScale3D(FVector(LintelDepth / BasicShapeSize, (DoorWidth + LintelOverhang) / BasicShapeSize, LintelHeight / BasicShapeSize));
	}
	// 실제 문 메시를 쓸 때는 문틀이 위를 덮으므로 상인방이 필요 없다.
	Lintel->SetVisibility(bFitBasicShapeDoors);
	Lintel->SetCollisionEnabled(bFitBasicShapeDoors ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	// 안개판(판 메시를 세운 것: X = 높이, Y = 폭)은 문짝 뒤 통로 안에 둔다.
	const double FogHeight = DoorHeight + FogMargin;
	FogWall->SetRelativeLocation(FVector(FogInset, 0.0, FogHeight * 0.5));
	FogWall->SetRelativeScale3D(FVector(FogHeight / BasicShapeSize, (DoorWidth + FogMargin) / BasicShapeSize, 1.0));

	InteractionTrigger->SetRelativeLocation(FVector(EntryPoint->GetRelativeLocation().X, 0.0, TriggerHalfHeight));
	InteractionTrigger->SetBoxExtent(FVector(TriggerHalfDepth, FMath::Max(HalfWidth + TriggerSideMargin, TriggerMinHalfWidth), TriggerHalfHeight));
}

void AODBossEntrance::BeginPlay()
{
	Super::BeginPlay();

	InteractionTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnTriggerBeginOverlap);
	InteractionTrigger->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnTriggerEndOverlap);
	SetDoorAlpha(0.0f);
	FogMaterial = FogWall->CreateDynamicMaterialInstance(0);
	SetFogAmount(1.0f);
}

void AODBossEntrance::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (IsValid(SequencePlayer))
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &ThisClass::OnSequenceFinished);
	}
	Super::EndPlay(EndPlayReason);
}

void AODBossEntrance::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateWalk(DeltaSeconds);
}

void AODBossEntrance::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (UODInteractionComponent* Interaction = IsValid(OtherActor) ? OtherActor->FindComponentByClass<UODInteractionComponent>() : nullptr)
	{
		Interaction->RegisterCandidate(this);
	}
}

void AODBossEntrance::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (UODInteractionComponent* Interaction = IsValid(OtherActor) ? OtherActor->FindComponentByClass<UODInteractionComponent>() : nullptr)
	{
		Interaction->UnregisterCandidate(this);
	}
}

bool AODBossEntrance::IsCleared() const
{
	const UWorld* World = GetWorld();
	const UODBossEncounterSubsystem* Encounter = World ? World->GetSubsystem<UODBossEncounterSubsystem>() : nullptr;
	if (!Encounter || Bosses.IsEmpty())
	{
		return false;
	}
	for (const TObjectPtr<AODEnemyCharacter>& Boss : Bosses)
	{
		if (!Encounter->IsBossDefeated(Boss))
		{
			return false;
		}
	}
	return true;
}

bool AODBossEntrance::CanInteract(const APawn* InstigatorPawn) const
{
	return !bUsed && !bInProgress && IsValid(InstigatorPawn);
}

void AODBossEntrance::Interact(APawn* InstigatorPawn)
{
	ACharacter* Character = Cast<ACharacter>(InstigatorPawn);
	if (!CanInteract(InstigatorPawn) || !IsValid(Character) || !GetWorld())
	{
		return;
	}

	bInProgress = true;
	EnteringPawn = InstigatorPawn;

	if (UODCombatModeComponent* ModeComponent = InstigatorPawn->FindComponentByClass<UODCombatModeComponent>())
	{
		ModeComponent->SetCinematicMode(true);
	}

	// 1) 문 정면 지점까지 걸어간 뒤 문 쪽으로 돌아선다. (끝나면 StartEntrySequence)
	StartWalkPhase(EWalkPhase::Approach);

	UE_LOG(LogOD, Log, TEXT("[%s] 보스방 입장 시작: %s"), *GetName(), *GetNameSafe(InstigatorPawn));
}

void AODBossEntrance::StartEntrySequence()
{
	UWorld* World = GetWorld();
	if (!World || !bInProgress)
	{
		return;
	}

	// 2) 입장 시퀀스 재생 (EntryPoint 기준)
	if (IsValid(EntrySequence))
	{
		FMovieSceneSequencePlaybackSettings Settings;
		// 걸어 들어가기는 이동 입력으로 하므로 이동 입력은 막지 않는다. (플레이어 조작은 컷신 모드에서 이미 막혀 있음)
		Settings.bDisableMovementInput = false;
		Settings.bDisableLookAtInput = true;

		ALevelSequenceActor* OutActor = nullptr;
		SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(World, EntrySequence, Settings, OutActor);
		SequenceActor = OutActor;
		if (IsValid(SequenceActor))
		{
			SequenceActor->bOverrideInstanceData = true;
			if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData))
			{
				InstanceData->TransformOrigin = FTransform(EntryPoint->GetComponentRotation(), EntryPoint->GetComponentLocation());
			}
		}
		if (IsValid(SequencePlayer))
		{
			SequencePlayer->OnFinished.AddUniqueDynamic(this, &ThisClass::OnSequenceFinished);
			SequencePlayer->Play();
		}
	}
	else
	{
		UE_LOG(LogOD, Warning, TEXT("[%s] 입장 시퀀스가 없어 카메라 연출 없이 입장합니다."), *GetName());
	}

	// 3) 문 열기 → 걸어 들어가기
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.SetTimer(DoorDelayTimer, FTimerDelegate::CreateUObject(this, &ThisClass::AnimateDoor, 1.0f), FMath::Max(DoorOpenDelay, KINDA_SMALL_NUMBER), false);
	TimerManager.SetTimer(WalkTimer, this, &ThisClass::StartWalkIn, FMath::Max(WalkInStartTime, KINDA_SMALL_NUMBER), false);
	// 문이 열려 안쪽 안개가 드러난 뒤, 걸어 들어가는 동안 걷힌다.
	TimerManager.SetTimer(FogDelayTimer, this, &ThisClass::StartFogFade, FMath::Max(FogFadeDelay, KINDA_SMALL_NUMBER), false);
}

void AODBossEntrance::StartFogFade()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FogFadeStartTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(FogFadeTimer, this, &ThisClass::UpdateFogFade, ODBossEntrance::DoorTickInterval, true);
}

void AODBossEntrance::UpdateFogFade()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float T = FMath::Clamp(static_cast<float>(World->GetTimeSeconds() - FogFadeStartTime) / FogFadeTime, 0.0f, 1.0f);
	SetFogAmount(1.0f - FMath::InterpEaseOut(0.0f, 1.0f, T, 2.0f));
	if (T >= 1.0f)
	{
		World->GetTimerManager().ClearTimer(FogFadeTimer);
	}
}

void AODBossEntrance::SetFogAmount(float Amount)
{
	if (IsValid(FogMaterial))
	{
		FogMaterial->SetScalarParameterValue(ODBossEntrance::FogFadeParameter, Amount);
	}
	FogWall->SetVisibility(Amount > 0.0f);
}

void AODBossEntrance::StartWalkIn()
{
	ACharacter* Character = Cast<ACharacter>(EnteringPawn.Get());
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !World)
	{
		return;
	}

	StartWalkPhase(EWalkPhase::WalkIn);

	// 시퀀스 카메라가 화면을 잡고 있는 동안 3인칭으로 바꿔 둔다. (끝나면 시퀀스 카메라에서 블렌드)
	if (UODCombatModeComponent* ModeComponent = Character->FindComponentByClass<UODCombatModeComponent>())
	{
		ModeComponent->SetCombatMode(ModeAfterEntry);
	}

	// 시퀀스가 없으면 걷기가 끝날 즈음 마무리한다.
	if (!IsValid(SequencePlayer))
	{
		World->GetTimerManager().SetTimer(FinishTimer, this, &ThisClass::FinishEntry, WalkPhaseTimeout, false);
	}
}

void AODBossEntrance::StartWalkPhase(EWalkPhase Phase)
{
	UWorld* World = GetWorld();
	const ACharacter* Character = Cast<ACharacter>(EnteringPawn.Get());
	if (!World || !IsValid(Character))
	{
		return;
	}

	WalkPhase = Phase;
	WalkPhaseStartTime = World->GetTimeSeconds();

	// 막힘 등으로 도착하지 못할 때를 대비한 제한 시간
	const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	const float Speed = MoveComp ? FMath::Max(MoveComp->GetMaxSpeed(), 50.0f) : 500.0f;
	const FVector Target = (Phase == EWalkPhase::WalkIn) ? RoomPoint->GetComponentLocation() : EntryPoint->GetComponentLocation();
	WalkPhaseTimeout = static_cast<float>(FVector::Dist2D(Character->GetActorLocation(), Target) / Speed) + 1.5f;

	SetActorTickEnabled(true);
}

void AODBossEntrance::StopWalk()
{
	WalkPhase = EWalkPhase::None;
	SetActorTickEnabled(false);
}

void AODBossEntrance::UpdateWalk(float DeltaSeconds)
{
	ACharacter* Character = Cast<ACharacter>(EnteringPawn.Get());
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !World || WalkPhase == EWalkPhase::None)
	{
		StopWalk();
		return;
	}

	const bool bTimedOut = World->GetTimeSeconds() - WalkPhaseStartTime > WalkPhaseTimeout;

	switch (WalkPhase)
	{
	case EWalkPhase::Approach:
	case EWalkPhase::WalkIn:
	{
		const FVector Target = (WalkPhase == EWalkPhase::WalkIn) ? RoomPoint->GetComponentLocation() : EntryPoint->GetComponentLocation();
		const FVector ToTarget = (Target - Character->GetActorLocation()) * FVector(1.0, 1.0, 0.0);
		// 프레임이 끊겨 한 걸음이 커지면 목표를 지나칠 수 있다. 가까이서 목표와 멀어지는 쪽으로 움직이면 도착으로 본다. (되돌아오며 몸이 뒤로 도는 것 방지)
		const bool bPassedTarget = ToTarget.Size() < 80.0 && FVector::DotProduct(ToTarget, Character->GetVelocity() * FVector(1.0, 1.0, 0.0)) < 0.0;
		if (ToTarget.Size() <= ArriveTolerance || bPassedTarget || bTimedOut)
		{
			if (WalkPhase == EWalkPhase::Approach)
			{
				// 정면 지점에 정확히 맞추고 멈춘 뒤 문 쪽으로 돈다.
				if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
				{
					MoveComp->StopMovementImmediately();
				}
				Character->SetActorLocation(FVector(Target.X, Target.Y, Character->GetActorLocation().Z), true);
				WalkPhase = EWalkPhase::Turn;
				WalkPhaseStartTime = World->GetTimeSeconds();
				WalkPhaseTimeout = 2.0f;
			}
			else
			{
				// 방 안 도착: 입력을 끊으면 이동 컴포넌트의 감속으로 자연스럽게 멈춘다.
				StopWalk();
			}
			return;
		}
		// 실제 이동 입력이라 이동 방향으로 몸이 돌고 걷기 애니메이션이 재생된다.
		// 목표 가까이에서는 입력 세기를 줄여(아날로그 입력 = 최대 속도 비례) 걷기로 감속하며 멈춘다. 지나쳐서 맴도는 것을 막는다.
		constexpr double SlowRadius = 150.0;
		const float InputScale = static_cast<float>(FMath::Clamp(ToTarget.Size() / SlowRadius, 0.25, 1.0));
		Character->AddMovementInput(ToTarget.GetSafeNormal(), InputScale);
		return;
	}
	case EWalkPhase::Turn:
	{
		const float DoorYaw = static_cast<float>(EntryPoint->GetComponentRotation().Yaw);
		const FRotator Current = Character->GetActorRotation();
		const FRotator Wanted(0.0f, DoorYaw, 0.0f);
		const FRotator Next = FMath::RInterpConstantTo(Current, Wanted, DeltaSeconds, TurnRate);
		Character->SetActorRotation(Next);
		if (FMath::Abs(FRotator::NormalizeAxis(Next.Yaw - DoorYaw)) < 1.0f || bTimedOut)
		{
			Character->SetActorRotation(Wanted);
			StopWalk();
			StartEntrySequence();
		}
		return;
	}
	default:
		StopWalk();
		return;
	}
}

void AODBossEntrance::OnSequenceFinished()
{
	FinishEntry();

	// 재생 콜백 안에서 액터를 지우지 않고 다음 틱에 정리한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsValid(SequenceActor))
			{
				SequenceActor->Destroy();
			}
			SequenceActor = nullptr;
			SequencePlayer = nullptr;
		}));
	}
}

void AODBossEntrance::FinishEntry()
{
	if (!bInProgress)
	{
		return;
	}
	bInProgress = false;
	bUsed = true;

	APawn* Pawn = EnteringPawn.Get();
	if (IsValid(Pawn))
	{
		if (UODCombatModeComponent* ModeComponent = Pawn->FindComponentByClass<UODCombatModeComponent>())
		{
			// 시퀀스 없이 입장한 경우 모드가 아직 안 바뀌었을 수 있다.
			ModeComponent->SetCombatMode(ModeAfterEntry);
			ModeComponent->SetCinematicMode(false);
		}
		if (!IsValid(SequencePlayer))
		{
			if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
			{
				PC->SetViewTargetWithBlend(Pawn, FallbackCameraBlendTime);
			}
		}
	}

	if (bCloseBehind)
	{
		AnimateDoor(0.0f);
	}

	UE_LOG(LogOD, Log, TEXT("[%s] 보스방 입장 완료"), *GetName());
	OnEntered.Broadcast(Pawn);

	// 보스전 시작 (대기 중이던 보스 활성화, HUD 보스 체력 바)
	if (!Bosses.IsEmpty())
	{
		if (UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr)
		{
			TArray<AODEnemyCharacter*> BossList;
			BossList.Reserve(Bosses.Num());
			for (AODEnemyCharacter* Boss : Bosses)
			{
				BossList.Add(Boss);
			}
			Encounter->OnEncounterEnded.AddUniqueDynamic(this, &ThisClass::HandleEncounterEnded);
			bEncounterStarted = true;
			Encounter->StartEncounter(BossList, this);
		}
	}
}

void AODBossEntrance::HandleEncounterEnded(bool bVictory)
{
	if (!bEncounterStarted)
	{
		return;
	}
	bEncounterStarted = false;

	if (UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr)
	{
		Encounter->OnEncounterEnded.RemoveDynamic(this, &ThisClass::HandleEncounterEnded);
	}

	// 승리하면 닫혀 있던 문을 다시 연다.
	if (bVictory && bOpenOnVictory)
	{
		AnimateDoor(1.0f);
	}
}

void AODBossEntrance::AnimateDoor(float TargetAlpha)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	DoorAlphaFrom = DoorAlpha;
	DoorAlphaTo = FMath::Clamp(TargetAlpha, 0.0f, 1.0f);
	DoorAnimStartTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(DoorTimer, this, &ThisClass::UpdateDoor, ODBossEntrance::DoorTickInterval, true);
}

void AODBossEntrance::UpdateDoor()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - DoorAnimStartTime);
	const float T = FMath::Clamp(Elapsed / DoorOpenTime, 0.0f, 1.0f);
	SetDoorAlpha(FMath::Lerp(DoorAlphaFrom, DoorAlphaTo, FMath::InterpEaseInOut(0.0f, 1.0f, T, 2.0f)));
	if (T >= 1.0f)
	{
		World->GetTimerManager().ClearTimer(DoorTimer);
	}
}

void AODBossEntrance::SetDoorAlpha(float Alpha)
{
	DoorAlpha = Alpha;
	// 두 문짝 모두 방 안쪽(+X)으로 열린다.
	LeftHinge->SetRelativeRotation(FRotator(0.0f, -DoorOpenAngle * Alpha, 0.0f));
	RightHinge->SetRelativeRotation(FRotator(0.0f, DoorOpenAngle * Alpha, 0.0f));
}

FTransform AODBossEntrance::GetEntryTransform() const
{
	return FTransform(FRotator(0.0f, EntryPoint->GetComponentRotation().Yaw, 0.0f), EntryPoint->GetComponentLocation());
}

void AODBossEntrance::ResetForTest()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DoorTimer);
		World->GetTimerManager().ClearTimer(DoorDelayTimer);
		World->GetTimerManager().ClearTimer(WalkTimer);
		World->GetTimerManager().ClearTimer(FinishTimer);
		World->GetTimerManager().ClearTimer(FogDelayTimer);
		World->GetTimerManager().ClearTimer(FogFadeTimer);
	}
	StopWalk();

	// 이 입구에서 시작한 보스전이 진행 중이면 패배로 끝낸다. (보스는 대기 상태로)
	if (bEncounterStarted)
	{
		if (UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr)
		{
			Encounter->EndEncounter(false);
		}
		bEncounterStarted = false;
	}

	if (IsValid(SequencePlayer))
	{
		SequencePlayer->OnFinished.RemoveDynamic(this, &ThisClass::OnSequenceFinished);
		SequencePlayer->Stop();
	}
	if (IsValid(SequenceActor))
	{
		SequenceActor->Destroy();
	}
	SequencePlayer = nullptr;
	SequenceActor = nullptr;

	// 보스를 처치한 입구: 문은 열린 채, 안개 없이, 다시 들어갈 수 없게 둔다. (부활해도 보스방은 비어 있다)
	if (IsCleared())
	{
		SetDoorAlpha(bOpenOnVictory ? 1.0f : 0.0f);
		SetFogAmount(0.0f);
		bUsed = true;
		bInProgress = false;
		EnteringPawn.Reset();
		return;
	}

	SetDoorAlpha(0.0f);
	SetFogAmount(1.0f);
	const bool bWasEntered = bUsed || bInProgress;
	bUsed = false;
	bInProgress = false;

	ACharacter* Character = Cast<ACharacter>(EnteringPawn.Get());
	if (bWasEntered && IsValid(Character))
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}
		const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Character->TeleportTo(EntryPoint->GetComponentLocation() + FVector(0.0, 0.0, HalfHeight + 2.0), EntryPoint->GetComponentRotation());

		if (UODCombatModeComponent* ModeComponent = Character->FindComponentByClass<UODCombatModeComponent>())
		{
			ModeComponent->SetCinematicMode(false);
			ModeComponent->SetCombatMode(EODCombatMode::Field);
		}
	}
	EnteringPawn.Reset();
}
