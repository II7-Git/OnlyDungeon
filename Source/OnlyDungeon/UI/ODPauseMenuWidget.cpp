// OnlyDungeon 일시정지 메뉴

#include "ODPauseMenuWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/ODGameMode.h"
#include "Core/ODSaveSubsystem.h"
#include "UI/ODUIStyle.h"

#define LOCTEXT_NAMESPACE "ODPause"

namespace ODPause
{
	constexpr float ButtonWidth = 320.0f;
	constexpr float SliderWidth = 220.0f;
	constexpr float LabelWidth = 100.0f;
}

bool UODPauseMenuWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bSuperInitialized;
}

void UODPauseMenuWidget::BuildDefaultLayout()
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
	auto AddButton = [&](const FText& Label, float Gap) -> UButton*
	{
		UButton* Button = MakeButton(Tree, Label);
		AddRow(WithMinWidth(Tree, Button, ODPause::ButtonWidth), Gap);
		return Button;
	};
	auto AddSlider = [&](const FText& Label, TObjectPtr<USlider>& OutSlider)
	{
		UHorizontalBox* Line = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(ODPause::LabelWidth);
		LabelBox->AddChild(MakeText(Tree, Label, 18));
		Line->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

		USizeBox* SliderBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SliderBox->SetWidthOverride(ODPause::SliderWidth);
		OutSlider = Tree->ConstructWidget<USlider>(USlider::StaticClass());
		OutSlider->SetMinValue(0.0f);
		OutSlider->SetMaxValue(1.0f);
		OutSlider->SetStepSize(0.05f);
		OutSlider->SetSliderBarColor(FLinearColor(0.25f, 0.22f, 0.18f));
		OutSlider->SetSliderHandleColor(AccentColor);
		SliderBox->AddChild(OutSlider);
		Line->AddChildToHorizontalBox(SliderBox)->SetVerticalAlignment(VAlign_Center);
		AddRow(Line, 8.0f);
	};

	AddRow(MakeText(Tree, LOCTEXT("Title", "일시정지"), 40, AccentColor), 0.0f);
	AddButton(LOCTEXT("Resume", "계속하기"), 40.0f)->OnClicked.AddDynamic(this, &ThisClass::HandleResumeClicked);

	AddRow(MakeText(Tree, LOCTEXT("Volume", "볼륨"), 20), 30.0f);
	AddSlider(LOCTEXT("Music", "배경음"), MusicSlider);
	AddSlider(LOCTEXT("SFX", "효과음"), SFXSlider);
	AddSlider(LOCTEXT("Ambience", "환경음"), AmbienceSlider);
	MusicSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleMusicChanged);
	SFXSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleSFXChanged);
	AmbienceSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleAmbienceChanged);

	AddButton(LOCTEXT("ToTitle", "타이틀로"), 30.0f)->OnClicked.AddDynamic(this, &ThisClass::HandleTitleClicked);
	AddButton(LOCTEXT("Quit", "게임 종료"), 10.0f)->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);
}

void UODPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// 열 때마다 저장된 설정으로 슬라이더를 맞춘다.
	const UGameInstance* GameInstance = GetGameInstance();
	if (const UODSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UODSaveSubsystem>() : nullptr)
	{
		if (IsValid(MusicSlider)) { MusicSlider->SetValue(Save->GetVolume(EODVolumeChannel::Music)); }
		if (IsValid(SFXSlider)) { SFXSlider->SetValue(Save->GetVolume(EODVolumeChannel::SFX)); }
		if (IsValid(AmbienceSlider)) { AmbienceSlider->SetValue(Save->GetVolume(EODVolumeChannel::Ambience)); }
	}
}

void UODPauseMenuWidget::NativeDestruct()
{
	// 닫힐 때 설정을 파일에 쓴다. (슬라이더를 움직일 때마다 쓰지 않도록)
	const UGameInstance* GameInstance = GetGameInstance();
	if (UODSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UODSaveSubsystem>() : nullptr)
	{
		Save->SaveSettings();
	}
	Super::NativeDestruct();
}

void UODPauseMenuWidget::HandleResumeClicked()
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->SetMenuPaused(false);
	}
}

void UODPauseMenuWidget::HandleTitleClicked()
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->ReturnToTitle();
	}
}

void UODPauseMenuWidget::HandleQuitClicked()
{
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->QuitGame();
	}
}

void UODPauseMenuWidget::HandleMusicChanged(float Value)
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UODSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UODSaveSubsystem>() : nullptr)
	{
		Save->SetVolume(EODVolumeChannel::Music, Value);
	}
}

void UODPauseMenuWidget::HandleSFXChanged(float Value)
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UODSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UODSaveSubsystem>() : nullptr)
	{
		Save->SetVolume(EODVolumeChannel::SFX, Value);
	}
}

void UODPauseMenuWidget::HandleAmbienceChanged(float Value)
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UODSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UODSaveSubsystem>() : nullptr)
	{
		Save->SetVolume(EODVolumeChannel::Ambience, Value);
	}
}

#undef LOCTEXT_NAMESPACE
