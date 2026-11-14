// OnlyDungeon HUD 액터

#include "ODHUD.h"

#include "GameFramework/PlayerController.h"
#include "UI/ODHUDWidget.h"
#include "Core/ODLogChannels.h"

AODHUD::AODHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AODHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}

	const TSubclassOf<UODHUDWidget> WidgetClass = HUDWidgetClass ? HUDWidgetClass : TSubclassOf<UODHUDWidget>(UODHUDWidget::StaticClass());
	HUDWidget = CreateWidget<UODHUDWidget>(PC, WidgetClass);
	if (!IsValid(HUDWidget))
	{
		UE_LOG(LogOD, Warning, TEXT("[%s] HUD 위젯을 만들지 못했습니다."), *GetName());
		return;
	}

	HUDWidget->AddToViewport();
	HUDWidget->SetObservedPawn(PC->GetPawn());
	PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
}

void AODHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PC = GetOwningPlayerController(); IsValid(PC))
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
	if (IsValid(HUDWidget))
	{
		HUDWidget->RemoveFromParent();
	}
	HUDWidget = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AODHUD::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (IsValid(HUDWidget))
	{
		HUDWidget->SetObservedPawn(NewPawn);
	}
}
