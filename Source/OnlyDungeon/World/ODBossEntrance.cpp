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
#include "GameFramework/RootMotionSource.h"
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
	const FName WalkSourceName(TEXT("ODBossEntranceWalk"));
	constexpr float DoorTickInterval = 1.0f / 60.0f;
}

AODBossEntrance::AODBossEntrance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

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
	UWorld* World = GetWorld();
	if (!CanInteract(InstigatorPawn) || !IsValid(Character) || !World)
	{
		return;
	}

	bInProgress = true;
	EnteringPawn = InstigatorPawn;

	UODCombatModeComponent* ModeComponent = InstigatorPawn->FindComponentByClass<UODCombatModeComponent>();
	if (IsValid(ModeComponent))
	{
		ModeComponent->SetCinematicMode(true);
	}

	// 1) 문 앞 지점으로 걸어간다.
	const FVector EntryLocation = EntryPoint->GetComponentLocation();
	const double ToEntry = FVector::Dist2D(Character->GetActorLocation(), EntryLocation);
	const float ApproachTime = static_cast<float>(ToEntry / WalkSpeed);
	if (ToEntry > 5.0)
	{
		MoveCharacterTo(Character, EntryLocation, ApproachTime);
	}

	// 2) 입장 시퀀스 재생 (EntryPoint 기준)
	if (IsValid(EntrySequence))
	{
		FMovieSceneSequencePlaybackSettings Settings;
		Settings.bDisableMovementInput = true;
		Settings.bDisableLookAtInput = true;

		ALevelSequenceActor* OutActor = nullptr;
		SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(World, EntrySequence, Settings, OutActor);
		SequenceActor = OutActor;
		if (IsValid(SequenceActor))
		{
			SequenceActor->bOverrideInstanceData = true;
			if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData))
			{
				InstanceData->TransformOrigin = FTransform(EntryPoint->GetComponentRotation(), EntryLocation);
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
	TimerManager.SetTimer(WalkTimer, this, &ThisClass::StartWalkIn, FMath::Max3(WalkInStartTime, ApproachTime, KINDA_SMALL_NUMBER), false);

	UE_LOG(LogOD, Log, TEXT("[%s] 보스방 입장 시작: %s"), *GetName(), *GetNameSafe(InstigatorPawn));
}

void AODBossEntrance::StartWalkIn()
{
	ACharacter* Character = Cast<ACharacter>(EnteringPawn.Get());
	UWorld* World = GetWorld();
	if (!IsValid(Character) || !World)
	{
		return;
	}

	const FVector RoomLocation = RoomPoint->GetComponentLocation();
	const float WalkTime = static_cast<float>(FVector::Dist2D(Character->GetActorLocation(), RoomLocation) / WalkSpeed);
	MoveCharacterTo(Character, RoomLocation, WalkTime);

	// 시퀀스 카메라가 화면을 잡고 있는 동안 3인칭으로 바꿔 둔다. (끝나면 시퀀스 카메라에서 블렌드)
	if (UODCombatModeComponent* ModeComponent = Character->FindComponentByClass<UODCombatModeComponent>())
	{
		ModeComponent->SetCombatMode(ModeAfterEntry);
	}

	// 시퀀스가 없으면 걷기가 끝날 때 마무리한다.
	if (!IsValid(SequencePlayer))
	{
		World->GetTimerManager().SetTimer(FinishTimer, this, &ThisClass::FinishEntry, WalkTime + 0.2f, false);
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

void AODBossEntrance::MoveCharacterTo(ACharacter* Character, const FVector& GroundTarget, float Duration) const
{
	UCharacterMovementComponent* MoveComp = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (!MoveComp)
	{
		return;
	}

	const FVector Start = Character->GetActorLocation();
	const FVector Target(GroundTarget.X, GroundTarget.Y, Start.Z);
	const FVector Direction = (Target - Start).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		Character->SetActorRotation(Direction.Rotation());
	}

	MoveComp->RemoveRootMotionSource(ODBossEntrance::WalkSourceName);

	// 이동 속도만큼 속도가 생기므로 이동 애니메이션(조깅)이 자연스럽게 재생된다.
	TSharedPtr<FRootMotionSource_MoveToForce> MoveTo = MakeShared<FRootMotionSource_MoveToForce>();
	MoveTo->InstanceName = ODBossEntrance::WalkSourceName;
	MoveTo->AccumulateMode = ERootMotionAccumulateMode::Override;
	MoveTo->Priority = 900;
	MoveTo->StartLocation = Start;
	MoveTo->TargetLocation = Target;
	MoveTo->Duration = FMath::Max(Duration, 0.05f);
	MoveTo->bRestrictSpeedToExpected = true;
	MoveTo->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
	MoveTo->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
	MoveComp->ApplyRootMotionSource(MoveTo);
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
			MoveComp->RemoveRootMotionSource(ODBossEntrance::WalkSourceName);
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
