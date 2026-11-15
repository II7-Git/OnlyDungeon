// OnlyDungeon 리스폰 포인트

#include "ODRespawnPoint.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameMode.h"
#include "Interaction/ODInteractionComponent.h"

AODRespawnPoint::AODRespawnPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(SceneRoot);
	MarkerMesh->SetCollisionProfileName(TEXT("NoCollision"));
	MarkerMesh->SetCanEverAffectNavigation(false);

	InteractionTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionTrigger"));
	InteractionTrigger->SetupAttachment(SceneRoot);
	InteractionTrigger->SetBoxExtent(FVector(250.0, 250.0, 150.0));
	InteractionTrigger->SetRelativeLocation(FVector(0.0, 0.0, 150.0));
	InteractionTrigger->SetCollisionProfileName(TEXT("Trigger"));

	SpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(SceneRoot);
	SpawnPoint->SetRelativeLocation(FVector(-150.0, 0.0, 0.0));
	SpawnPoint->ArrowColor = FColor::Cyan;

	InteractionText = NSLOCTEXT("OnlyDungeon", "RespawnPointRest", "휴식");
}

void AODRespawnPoint::BeginPlay()
{
	Super::BeginPlay();
	InteractionTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnTriggerBeginOverlap);
	InteractionTrigger->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnTriggerEndOverlap);
}

void AODRespawnPoint::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (UODInteractionComponent* Interaction = IsValid(OtherActor) ? OtherActor->FindComponentByClass<UODInteractionComponent>() : nullptr)
	{
		Interaction->RegisterCandidate(this);
	}
}

void AODRespawnPoint::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (UODInteractionComponent* Interaction = IsValid(OtherActor) ? OtherActor->FindComponentByClass<UODInteractionComponent>() : nullptr)
	{
		Interaction->UnregisterCandidate(this);
	}
}

bool AODRespawnPoint::CanInteract(const APawn* InstigatorPawn) const
{
	const AODCharacterBase* Character = Cast<AODCharacterBase>(InstigatorPawn);
	return IsValid(Character) && !Character->IsDead();
}

void AODRespawnPoint::Interact(APawn* InstigatorPawn)
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->RestAtRespawnPoint(this, InstigatorPawn);
	}
}

FTransform AODRespawnPoint::GetRespawnTransform() const
{
	return FTransform(FRotator(0.0f, SpawnPoint->GetComponentRotation().Yaw, 0.0f), SpawnPoint->GetComponentLocation());
}
