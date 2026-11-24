// OnlyDungeon 클리어 화면

#include "ODClearWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Core/ODGameMode.h"
#include "UI/ODUIStyle.h"

#define LOCTEXT_NAMESPACE "ODClear"

namespace ODClear
{
	constexpr float ButtonWidth = 320.0f;
	constexpr int32 SecondsPerMinute = 60;
	constexpr int32 SecondsPerHour = 3600;
}

bool UODClearWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bSuperInitialized;
}

void UODClearWidget::BuildDefaultLayout()
{
	using namespace ODUIStyle;
	UWidgetTree* Tree = WidgetTree;

	UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Tree->RootWidget = Canvas;

	UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Dim->SetBrushColor(DimColor);
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

	AddRow(MakeText(Tree, LOCTEXT("Title", "던전 클리어"), 56, AccentColor), 0.0f);
	AddRow(MakeText(Tree, LOCTEXT("Sub", "돌의 수호자가 쓰러졌다"), 20), 6.0f);
	StatsText = MakeText(Tree, FText::GetEmpty(), 22);
	AddRow(StatsText, 36.0f);

	UButton* ContinueButton = MakeButton(Tree, LOCTEXT("Continue", "계속 탐험"));
	ContinueButton->OnClicked.AddDynamic(this, &ThisClass::HandleContinueClicked);
	AddRow(WithMinWidth(Tree, ContinueButton, ODClear::ButtonWidth), 40.0f);

	UButton* TitleButton = MakeButton(Tree, LOCTEXT("ToTitle", "타이틀로"));
	TitleButton->OnClicked.AddDynamic(this, &ThisClass::HandleTitleClicked);
	AddRow(WithMinWidth(Tree, TitleButton, ODClear::ButtonWidth), 10.0f);
}

void UODClearWidget::NativeConstruct()
{
	Super::NativeConstruct();
	const AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr;
	if (!IsValid(StatsText) || !GameMode)
	{
		return;
	}
	const FODRunStats Stats = GameMode->GetRunStats();
	const int32 Total = FMath::FloorToInt32(Stats.PlayTimeSeconds);
	const int32 Hours = Total / ODClear::SecondsPerHour;
	const int32 Minutes = (Total % ODClear::SecondsPerHour) / ODClear::SecondsPerMinute;
	const int32 Seconds = Total % ODClear::SecondsPerMinute;
	StatsText->SetText(FText::Format(LOCTEXT("Stats", "플레이 시간 {0}:{1}:{2}    사망 {3}회"),
		FText::AsNumber(Hours), FText::FromString(FString::Printf(TEXT("%02d"), Minutes)), FText::FromString(FString::Printf(TEXT("%02d"), Seconds)),
		FText::AsNumber(Stats.DeathCount)));
}

void UODClearWidget::HandleContinueClicked()
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->ContinueAfterClear();
	}
}

void UODClearWidget::HandleTitleClicked()
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->ReturnToTitle();
	}
}

#undef LOCTEXT_NAMESPACE
