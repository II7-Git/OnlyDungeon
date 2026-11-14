// OnlyDungeon 상호작용 컴포넌트 (플레이어)

#include "ODInteractionComponent.h"

#include "GameFramework/Pawn.h"
#include "Interaction/ODInteractable.h"
#include "Core/ODLogChannels.h"

UODInteractionComponent::UODInteractionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UODInteractionComponent::RegisterCandidate(AActor* Candidate)
{
	if (!IsValid(Candidate) || !Candidate->Implements<UODInteractable>())
	{
		return;
	}
	Candidates.AddUnique(Candidate);
	RefreshFocus();
}

void UODInteractionComponent::UnregisterCandidate(AActor* Candidate)
{
	Candidates.Remove(Candidate);
	RefreshFocus();
}

void UODInteractionComponent::RefreshFocus()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn))
	{
		return;
	}

	Candidates.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr) { return !Ptr.IsValid(); });

	AActor* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const TWeakObjectPtr<AActor>& Ptr : Candidates)
	{
		AActor* Candidate = Ptr.Get();
		const IODInteractable* Interactable = Cast<IODInteractable>(Candidate);
		if (!Interactable || !Interactable->CanInteract(OwnerPawn))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Candidate->GetActorLocation(), OwnerPawn->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}

	if (FocusedActor.Get() != Best)
	{
		FocusedActor = Best;
		OnFocusChanged.Broadcast(Best);
	}
}

bool UODInteractionComponent::TryInteract()
{
	RefreshFocus();

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AActor* Target = FocusedActor.Get();
	IODInteractable* Interactable = Cast<IODInteractable>(Target);
	if (!IsValid(OwnerPawn) || !Interactable || !Interactable->CanInteract(OwnerPawn))
	{
		return false;
	}

	UE_LOG(LogOD, Log, TEXT("%s 상호작용: %s"), *GetNameSafe(OwnerPawn), *GetNameSafe(Target));
	Interactable->Interact(OwnerPawn);
	RefreshFocus();
	return true;
}

FText UODInteractionComponent::GetFocusedInteractionText() const
{
	const IODInteractable* Interactable = Cast<IODInteractable>(FocusedActor.Get());
	return Interactable ? Interactable->GetInteractionText() : FText::GetEmpty();
}
