// OnlyDungeon 보스방 입구 (문 + 입장 컷신)

#include "ODBossEntrance.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
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
#include "Core/ODLogChannels.h"
#include "Interaction/ODInteractionComponent.h"

namespace ODBossEntrance
{
	constexpr float DoorTickInterval = 1.0f / 60.0f;
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
	LeftHinge->SetRelativeLocation(FVector(0.0, -150.0, 0.0));

	LeftDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoor"));
	LeftDoor->SetupAttachment(LeftHinge);
	LeftDoor->SetRelativeLocation(FVector(0.0, 75.0, 200.0));
	LeftDoor->SetRelativeScale3D(FVector(0.2, 1.5, 4.0));

	RightHinge = CreateDefaultSubobject<USceneComponent>(TEXT("RightHinge"));
	RightHinge->SetupAttachment(SceneRoot);
	RightHinge->SetRelativeLocation(FVector(0.0, 150.0, 0.0));

	RightDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoor"));
	RightDoor->SetupAttachment(RightHinge);
	RightDoor->SetRelativeLocation(FVector(0.0, -75.0, 200.0));
	RightDoor->SetRelativeScale3D(FVector(0.2, 1.5, 4.0));

	Lintel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lintel"));
	Lintel->SetupAttachment(SceneRoot);
	Lintel->SetRelativeLocation(FVector(0.0, 0.0, 450.0));
	Lintel->SetRelativeScale3D(FVector(0.4, 4.0, 1.0));

	InteractionTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionTrigger"));
	InteractionTrigger->SetupAttachment(SceneRoot);
	InteractionTrigger->SetRelativeLocation(FVector(-250.0, 0.0, 150.0));
	InteractionTrigger->SetBoxExtent(FVector(200.0, 300.0, 150.0));
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
}

void AODBossEntrance::BeginPlay()
{
	Super::BeginPlay();

	InteractionTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnTriggerBeginOverlap);
	InteractionTrigger->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnTriggerEndOverlap);
	SetDoorAlpha(0.0f);
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

void AODBossEntrance::ResetForTest()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DoorTimer);
		World->GetTimerManager().ClearTimer(DoorDelayTimer);
		World->GetTimerManager().ClearTimer(WalkTimer);
		World->GetTimerManager().ClearTimer(FinishTimer);
	}
	StopWalk();
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

	SetDoorAlpha(0.0f);
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
