// OnlyDungeon 회복 물약 (개수 관리)

#include "ODPotionComponent.h"

#include "Core/ODLogChannels.h"

UODPotionComponent::UODPotionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UODPotionComponent::BeginPlay()
{
	Super::BeginPlay();
	SetCharges(MaxCharges);
}

bool UODPotionComponent::TryConsume()
{
	if (CurrentCharges <= 0)
	{
		return false;
	}
	SetCharges(CurrentCharges - 1);
	return true;
}

void UODPotionComponent::Refill()
{
	SetCharges(MaxCharges);
}

void UODPotionComponent::SetCharges(int32 NewCharges)
{
	NewCharges = FMath::Clamp(NewCharges, 0, MaxCharges);
	if (NewCharges == CurrentCharges)
	{
		// 처음 BeginPlay처럼 값이 같아도 HUD가 초기 값을 받도록 알린다.
		OnChargesChanged.Broadcast(CurrentCharges, MaxCharges);
		return;
	}
	CurrentCharges = NewCharges;
	UE_LOG(LogODAbility, Verbose, TEXT("%s 물약 %d/%d"), *GetNameSafe(GetOwner()), CurrentCharges, MaxCharges);
	OnChargesChanged.Broadcast(CurrentCharges, MaxCharges);
}
