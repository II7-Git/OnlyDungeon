// OnlyDungeon 타이틀 화면

#include "ODTitleWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/ODGameMode.h"
#include "Core/ODSaveSubsystem.h"
#include "UI/ODUIStyle.h"

#define LOCTEXT_NAMESPACE "ODTitle"

namespace ODTitle
{
	constexpr float ButtonWidth = 320.0f;
	constexpr float ButtonGap = 10.0f;
}

bool UODTitleWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bSuperInitialized;
}

void UODTitleWidget::BuildDefaultLayout()
{
	using namespace ODUIStyle;
	UWidgetTree* Tree = WidgetTree;

	UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Tree->RootWidget = Canvas;

	// 화면 전체를 어둡게 덮는다. (뒤로 멈춘 숲이 희미하게 보인다)
	UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f));
	UCanvasPanelSlot* DimSlot = Canvas->AddChildToCanvas(Dim);
	DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DimSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCanvasPanelSlot* ColumnSlot = Canvas->AddChildToCanvas(Column);
	ColumnSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	ColumnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ColumnSlot->SetAutoSize(true);

	auto AddRow = [Column](UWidget* Widget, float Gap)
	{
		UVerticalBoxSlot* Row = Column->AddChildToVerticalBox(Widget);
		Row->SetHorizontalAlignment(HAlign_Center);
		Row->SetPadding(FMargin(0.0f, Gap, 0.0f, 0.0f));
	};

	AddRow(MakeText(Tree, LOCTEXT("Title", "ONLY DUNGEON"), 64, AccentColor), 0.0f);
	AddRow(MakeText(Tree, LOCTEXT("Subtitle", "황폐한 숲 끝, 돌의 수호자"), 20), 6.0f);

	auto MakeMenu = [&](TObjectPtr<UVerticalBox>& OutBox)
	{
		OutBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddRow(OutBox, 60.0f);
	};
	auto AddButton = [&](UVerticalBox* Box, const FText& Label) -> UButton*
	{
		UButton* Button = MakeButton(Tree, Label);
		UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(WithMinWidth(Tree, Button, ODTitle::ButtonWidth));
		ButtonSlot->SetPadding(FMargin(0.0f, ODTitle::ButtonGap, 0.0f, 0.0f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		return Button;
	};

	MakeMenu(MainButtons);
	AddButton(MainButtons, LOCTEXT("NewGame", "새 게임"))->OnClicked.AddDynamic(this, &ThisClass::HandleNewGameClicked);
	ContinueButton = AddButton(MainButtons, LOCTEXT("Continue", "이어하기"));
	ContinueButton->OnClicked.AddDynamic(this, &ThisClass::HandleContinueClicked);
	AddButton(MainButtons, LOCTEXT("Quit", "종료"))->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);

	MakeMenu(ConfirmButtons);
	ConfirmButtons->AddChildToVerticalBox(MakeText(Tree, LOCTEXT("ConfirmText", "저장된 진행이 사라집니다. 새로 시작할까요?"), 20));
	AddButton(ConfirmButtons, LOCTEXT("ConfirmYes", "새로 시작"))->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmNewGameClicked);
	AddButton(ConfirmButtons, LOCTEXT("ConfirmNo", "취소"))->OnClicked.AddDynamic(this, &ThisClass::HandleCancelClicked);
}

void UODTitleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshButtons();
}

void UODTitleWidget::RefreshButtons()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UODSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UODSaveSubsystem>() : nullptr;
	if (IsValid(ContinueButton))
	{
		ContinueButton->SetIsEnabled(Save && Save->HasProgress());
	}
	SetConfirmVisible(false);
}

void UODTitleWidget::SetConfirmVisible(bool bVisible)
{
	if (IsValid(MainButtons))
	{
		MainButtons->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (IsValid(ConfirmButtons))
	{
		ConfirmButtons->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UODTitleWidget::HandleNewGameClicked()
{
	// 이어할 저장이 있으면 덮어쓰기 전에 한 번 더 묻는다.
	if (IsValid(ContinueButton) && ContinueButton->GetIsEnabled())
	{
		SetConfirmVisible(true);
		return;
	}
	HandleConfirmNewGameClicked();
}

void UODTitleWidget::HandleConfirmNewGameClicked()
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->StartNewGame();
	}
}

void UODTitleWidget::HandleContinueClicked()
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->ContinueGame();
	}
}

void UODTitleWidget::HandleQuitClicked()
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->QuitGame();
	}
}

void UODTitleWidget::HandleCancelClicked()
{
	SetConfirmVisible(false);
}

#undef LOCTEXT_NAMESPACE
