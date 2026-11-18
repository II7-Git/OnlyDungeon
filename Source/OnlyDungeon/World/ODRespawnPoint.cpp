// OnlyDungeon 리스폰 포인트

#include "ODRespawnPoint.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameMode.h"
#include "Core/ODGameplayTags.h"
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
	if (!IsValid(Character) || Character->IsDead())
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	return !IsValid(ASC) || !ASC->HasMatchingGameplayTag(ODGameplayTags::State_Resting);
}

void AODRespawnPoint::Interact(APawn* InstigatorPawn)
{
	// 휴식 어빌리티가 있으면 모션 중간에 회복한다. (어빌리티가 게임 모드의 휴식 처리를 부른다)
	if (const AODCharacterBase* Character = Cast<AODCharacterBase>(InstigatorPawn); IsValid(Character))
	{
		if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent(); IsValid(ASC))
		{
			FGameplayEventData Payload;
			Payload.EventTag = ODGameplayTags::Event_Rest;
			Payload.Instigator = InstigatorPawn;
			Payload.OptionalObject = this;
			if (ASC->HandleGameplayEvent(ODGameplayTags::Event_Rest, &Payload) > 0)
			{
				return;
			}
		}
	}

	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->RestAtRespawnPoint(this, InstigatorPawn);
	}
}

FTransform AODRespawnPoint::GetRespawnTransform() const
{
	return FTransform(FRotator(0.0f, SpawnPoint->GetComponentRotation().Yaw, 0.0f), SpawnPoint->GetComponentLocation());
}
