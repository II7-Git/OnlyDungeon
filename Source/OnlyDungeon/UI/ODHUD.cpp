// OnlyDungeon HUD 액터

#include "ODHUD.h"

#include "GameFramework/PlayerController.h"
#include "Character/ODCombatModeComponent.h"
#include "Engine/World.h"
#include "UI/ODClearWidget.h"
#include "UI/ODHUDWidget.h"
#include "UI/ODPauseMenuWidget.h"
#include "UI/ODTitleWidget.h"
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

	// 게임 흐름·일시정지 알림을 구독하고, 지금 상태(이미 타이틀일 수 있다)를 바로 반영한다.
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->OnGameFlowChanged.AddUniqueDynamic(this, &ThisClass::HandleGameFlowChanged);
		GameMode->OnMenuPauseChanged.AddUniqueDynamic(this, &ThisClass::HandleMenuPauseChanged);
		HandleGameFlowChanged(GameMode->GetFlowState());
	}
}

void AODHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PC = GetOwningPlayerController(); IsValid(PC))
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->OnGameFlowChanged.RemoveDynamic(this, &ThisClass::HandleGameFlowChanged);
		GameMode->OnMenuPauseChanged.RemoveDynamic(this, &ThisClass::HandleMenuPauseChanged);
	}
	for (UUserWidget* Menu : { TitleWidget.Get(), PauseMenuWidget.Get(), ClearWidget.Get() })
	{
		if (IsValid(Menu))
		{
			Menu->RemoveFromParent();
		}
	}
	TitleWidget = nullptr;
	PauseMenuWidget = nullptr;
	ClearWidget = nullptr;

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

void AODHUD::HandleGameFlowChanged(EODGameFlowState NewState)
{
	CurrentFlow = NewState;
	bPauseMenuOpen = false;

	UUserWidget* Title = TitleWidget.Get();
	UUserWidget* Pause = PauseMenuWidget.Get();
	UUserWidget* Clear = ClearWidget.Get();
	SetMenuVisible(Title, TitleWidgetClass ? TitleWidgetClass.Get() : UODTitleWidget::StaticClass(), NewState == EODGameFlowState::Title);
	SetMenuVisible(Pause, PauseMenuWidgetClass ? PauseMenuWidgetClass.Get() : UODPauseMenuWidget::StaticClass(), false);
	SetMenuVisible(Clear, ClearWidgetClass ? ClearWidgetClass.Get() : UODClearWidget::StaticClass(), NewState == EODGameFlowState::Cleared);
	TitleWidget = Title;
	PauseMenuWidget = Pause;
	ClearWidget = Clear;
	RefreshInputMode();
}

void AODHUD::HandleMenuPauseChanged(bool bPaused)
{
	bPauseMenuOpen = bPaused;
	UUserWidget* Pause = PauseMenuWidget.Get();
	SetMenuVisible(Pause, PauseMenuWidgetClass ? PauseMenuWidgetClass.Get() : UODPauseMenuWidget::StaticClass(), bPaused);
	PauseMenuWidget = Pause;
	RefreshInputMode();
}

void AODHUD::SetMenuVisible(UUserWidget*& Widget, UClass* WidgetClass, bool bVisible)
{
	if (!bVisible)
	{
		if (IsValid(Widget) && Widget->IsInViewport())
		{
			Widget->RemoveFromParent();
		}
		return;
	}
	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(Widget) && IsValid(PC) && WidgetClass)
	{
		Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	}
	if (IsValid(Widget) && !Widget->IsInViewport())
	{
		// HUD(0)보다 위에 그린다.
		constexpr int32 MenuZOrder = 10;
		Widget->AddToViewport(MenuZOrder);
	}
}

void AODHUD::RefreshInputMode()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!IsValid(PC))
	{
		return;
	}

	const bool bFullScreenMenu = CurrentFlow != EODGameFlowState::Playing;
	const bool bAnyMenu = bFullScreenMenu || bPauseMenuOpen;

	// 타이틀·클리어 화면에서는 전투 HUD를 숨긴다. (일시정지 메뉴는 HUD 위에 겹친다)
	if (IsValid(HUDWidget))
	{
		HUDWidget->SetVisibility(bFullScreenMenu ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (bAnyMenu)
	{
		// 메뉴 클릭을 받으면서도 일시정지 키(게임 입력)는 계속 받도록 GameAndUI로 둔다.
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
		return;
	}

	// 메뉴가 모두 닫히면 지금 전투 모드(필드 커서 / 보스 3인칭)의 입력 설정으로 되돌린다.
	if (const APawn* Pawn = PC->GetPawn())
	{
		if (UODCombatModeComponent* ModeComponent = Pawn->FindComponentByClass<UODCombatModeComponent>())
		{
			ModeComponent->ReapplyCurrentMode();
			return;
		}
	}
	PC->SetInputMode(FInputModeGameOnly());
	PC->SetShowMouseCursor(false);
}
