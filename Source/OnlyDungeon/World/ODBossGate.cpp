// OnlyDungeon 보스방 출구 문

#include "ODBossGate.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODLogChannels.h"
#include "Interaction/ODInteractionComponent.h"
#include "World/ODBossEntrance.h"

namespace ODBossGate
{
	/** 여는 동안 문짝 각도 갱신 간격 */
	constexpr float OpenTickInterval = 1.0f / 30.0f;

	/** 막이 충돌 두께의 절반 */
	constexpr float BlockerHalfThickness = 30.0f;

	/** 여닫이 가감속 곡선 지수 */
	constexpr float OpenEaseExponent = 2.0f;
}

AODBossGate::AODBossGate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(SceneRoot);

	LeftHinge = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHinge"));
	LeftHinge->SetupAttachment(SceneRoot);
	LeftDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoor"));
	LeftDoor->SetupAttachment(LeftHinge);

	RightHinge = CreateDefaultSubobject<USceneComponent>(TEXT("RightHinge"));
	RightHinge->SetupAttachment(SceneRoot);
	RightDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoor"));
	RightDoor->SetupAttachment(RightHinge);

	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(SceneRoot);
	Blocker->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	InteractionTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionTrigger"));
	InteractionTrigger->SetupAttachment(SceneRoot);
	InteractionTrigger->SetCollisionProfileName(TEXT("Trigger"));

	InteractionText = NSLOCTEXT("OnlyDungeon", "BossGateOpen", "문 열기");
}

void AODBossGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 문 앞은 +X(보스방 쪽), 폭은 Y축. 경첩은 문 양 끝에 둔다.
	const float HalfWidth = DoorWidth * 0.5f;
	LeftHinge->SetRelativeLocation(FVector(0.0, -HalfWidth, 0.0));
	RightHinge->SetRelativeLocation(FVector(0.0, HalfWidth, 0.0));

	Blocker->SetBoxExtent(FVector(ODBossGate::BlockerHalfThickness, HalfWidth, DoorHeight * 0.5f));
	Blocker->SetRelativeLocation(FVector(0.0, 0.0, DoorHeight * 0.5f));

	InteractionTrigger->SetBoxExtent(FVector(InteractionDepth * 0.5f, HalfWidth, DoorHeight * 0.5f));
	InteractionTrigger->SetRelativeLocation(FVector(InteractionDepth * 0.5f, 0.0, DoorHeight * 0.5f));
}

void AODBossGate::BeginPlay()
{
	Super::BeginPlay();
	InteractionTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnTriggerBeginOverlap);
	InteractionTrigger->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnTriggerEndOverlap);
	if (!IsValid(LinkedEntrance))
	{
		UE_LOG(LogOD, Warning, TEXT("%s: 연결된 보스방 입구(LinkedEntrance)가 없어 열리지 않습니다."), *GetName());
	}
	SetOpenAlpha(0.0f);
}

void AODBossGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OpenTimer);
	Super::EndPlay(EndPlayReason);
}

bool AODBossGate::IsUnlocked() const
{
	return IsValid(LinkedEntrance) && LinkedEntrance->IsCleared();
}

bool AODBossGate::CanInteract(const APawn* InstigatorPawn) const
{
	const AODCharacterBase* Character = Cast<AODCharacterBase>(InstigatorPawn);
	return IsValid(Character) && !Character->IsDead() && !IsOpen() && IsUnlocked();
}

void AODBossGate::Interact(APawn* InstigatorPawn)
{
	UWorld* World = GetWorld();
	if (!World || !CanInteract(InstigatorPawn))
	{
		return;
	}

	bOpening = true;
	OpenStartTime = World->GetTimeSeconds();
	World->GetTimerManager().SetTimer(OpenTimer, this, &ThisClass::UpdateOpen, ODBossGate::OpenTickInterval, true);
	if (IsValid(OpenSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetActorLocation());
	}
	UE_LOG(LogOD, Log, TEXT("%s 다음 지역 문을 엽니다."), *GetName());
}

void AODBossGate::UpdateOpen()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float T = FMath::Clamp(static_cast<float>(World->GetTimeSeconds() - OpenStartTime) / OpenDuration, 0.0f, 1.0f);
	SetOpenAlpha(FMath::InterpEaseInOut(0.0f, 1.0f, T, ODBossGate::OpenEaseExponent));
	if (T >= 1.0f)
	{
		GetWorldTimerManager().ClearTimer(OpenTimer);
		bOpening = false;
		bOpened = true;
	}
}

void AODBossGate::SetOpenAlpha(float Alpha)
{
	const float Angle = OpenAngle * Alpha;
	LeftHinge->SetRelativeRotation(FRotator(0.0, Angle, 0.0));
	RightHinge->SetRelativeRotation(FRotator(0.0, -Angle, 0.0));
	// 조금이라도 열리기 시작하면 지나갈 수 있게 막이를 끈다. (문짝이 길을 막지 않도록 문짝 충돌은 BP에서 끈다)
	Blocker->SetCollisionEnabled(Alpha > KINDA_SMALL_NUMBER ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

void AODBossGate::ResetForTest()
{
	GetWorldTimerManager().ClearTimer(OpenTimer);
	bOpening = false;
	bOpened = false;
	SetOpenAlpha(0.0f);
}

void AODBossGate::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (UODInteractionComponent* Interaction = IsValid(OtherActor) ? OtherActor->FindComponentByClass<UODInteractionComponent>() : nullptr)
	{
		Interaction->RegisterCandidate(this);
	}
}

void AODBossGate::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (UODInteractionComponent* Interaction = IsValid(OtherActor) ? OtherActor->FindComponentByClass<UODInteractionComponent>() : nullptr)
	{
		Interaction->UnregisterCandidate(this);
	}
}
