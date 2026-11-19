// OnlyDungeon 플레이어 HUD 위젯 (다크소울식 배치)

#include "ODHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "Character/ODCharacterBase.h"
#include "Character/ODCombatModeComponent.h"
#include "Character/ODEnemyCharacter.h"
#include "Core/ODBossEncounterSubsystem.h"
#include "Character/ODLockOnComponent.h"
#include "Character/ODPotionComponent.h"
#include "Interaction/ODInteractionComponent.h"
#include "Core/ODGameplayTags.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponData.h"

namespace ODHUD
{
	static TAutoConsoleVariable<bool> CVarShowDebug(
		TEXT("OD.Debug.HUD"),
		true,
		TEXT("HUD 오른쪽 위 테스트 정보(상태 태그, 몽타주, 강인도, 발동 실패 원인) 표시"));

	constexpr float LagTickInterval = 1.0f / 30.0f;
	constexpr float DebugTickInterval = 0.1f;
	constexpr float LockOnMarkerInterval = 1.0f / 60.0f;
	constexpr float BannerTickInterval = 0.03f;

	FSlateFontInfo Font(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

	UTextBlock* MakeText(UWidgetTree* Tree, int32 FontSize, const FLinearColor& Color, const FName Name = NAME_None)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(Font(FontSize));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0, 1.0));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		return Text;
	}

	/** 배경 없는 단색 진행 바 (프레임은 바깥 Border가 그린다) */
	UProgressBar* MakeBar(UWidgetTree* Tree, const FLinearColor& Color, const FName Name)
	{
		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.SetBackgroundImage(FSlateColorBrush(FLinearColor::Transparent));
		Style.SetFillImage(FSlateColorBrush(FLinearColor::White));
		Style.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(Color);
		Bar->SetPercent(1.0f);
		return Bar;
	}

	FString StateLabel(const FGameplayTag& Tag)
	{
		static const TMap<FName, FString> Labels = {
			{ TEXT("State.Attacking"), TEXT("공격 중") },
			{ TEXT("State.Dodging"), TEXT("회피 중") },
			{ TEXT("State.Defending"), TEXT("방어 중") },
			{ TEXT("State.Parrying"), TEXT("패링 판정") },
			{ TEXT("State.Cancelable"), TEXT("캔슬 가능") },
			{ TEXT("State.Invulnerable"), TEXT("무적") },
			{ TEXT("State.Stunned"), TEXT("경직") },
			{ TEXT("State.Dead"), TEXT("사망") },
			{ TEXT("State.Drinking"), TEXT("물약 마시는 중") },
			{ TEXT("State.LockOn"), TEXT("락온") },
		};
		const FString* Label = Labels.Find(Tag.GetTagName());
		return Label ? *Label : Tag.ToString();
	}
}

bool UODHUDWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (!bSuperInitialized || !WidgetTree)
	{
		return bSuperInitialized;
	}

	// WBP 디자이너가 비어 있으면(루트 없음 또는 빈 패널) 기본 레이아웃을 만든다.
	UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;
		BuildDefaultLayout(Canvas);
	}
	else if (RootPanel && RootPanel->GetChildrenCount() == 0)
	{
		UCanvasPanel* Canvas = Cast<UCanvasPanel>(RootPanel);
		if (!Canvas)
		{
			Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			RootPanel->AddChild(Canvas);
		}
		BuildDefaultLayout(Canvas);
	}

	return bSuperInitialized;
}

void UODHUDWidget::BuildDefaultLayout(UCanvasPanel* Canvas)
{
	using namespace ODHUD;
	UWidgetTree* Tree = WidgetTree;
	const FLinearColor TextColor(0.85f, 0.82f, 0.75f);

	// 바 하나: SizeBox(길이) → Overlay[프레임 Border, 지연 바, 채움 바]
	auto MakeBarRow = [&](const FName Prefix, const FLinearColor& Color, bool bWithLag, USizeBox*& OutBox, UProgressBar*& OutFill, UProgressBar*& OutLag) -> USizeBox*
	{
		OutBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(Prefix.ToString() + TEXT("Box")));
		OutBox->SetHeightOverride(BarHeight);
		OutBox->SetWidthOverride(300.0f);

		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		OutBox->AddChild(Overlay);

		UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrushColor(FrameColor);
		if (UOverlaySlot* FrameSlot = Overlay->AddChildToOverlay(Frame))
		{
			FrameSlot->SetHorizontalAlignment(HAlign_Fill);
			FrameSlot->SetVerticalAlignment(VAlign_Fill);
		}

		OutLag = nullptr;
		if (bWithLag)
		{
			OutLag = MakeBar(Tree, LagColor, *(Prefix.ToString() + TEXT("Lag")));
			if (UOverlaySlot* LagSlot = Overlay->AddChildToOverlay(OutLag))
			{
				LagSlot->SetHorizontalAlignment(HAlign_Fill);
				LagSlot->SetVerticalAlignment(VAlign_Fill);
				LagSlot->SetPadding(FMargin(2.0f));
			}
		}

		OutFill = MakeBar(Tree, Color, *(Prefix.ToString() + TEXT("Fill")));
		if (UOverlaySlot* FillSlot = Overlay->AddChildToOverlay(OutFill))
		{
			FillSlot->SetHorizontalAlignment(HAlign_Fill);
			FillSlot->SetVerticalAlignment(VAlign_Fill);
			FillSlot->SetPadding(FMargin(2.0f));
		}
		return OutBox;
	};

	// ---- 왼쪽 위: 체력·스태미나 ----
	{
		UVerticalBox* Bars = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayerBars"));
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Bars))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0, 0.0));
			CanvasSlot->SetPosition(FVector2D(60.0, 48.0));
			CanvasSlot->SetAutoSize(true);
		}

		auto AddRow = [&](USizeBox* Box, UTextBlock* Value)
		{
			UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			if (UHorizontalBoxSlot* BoxSlot = Row->AddChildToHorizontalBox(Box))
			{
				BoxSlot->SetVerticalAlignment(VAlign_Center);
			}
			if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Value))
			{
				TextSlot->SetVerticalAlignment(VAlign_Center);
				TextSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
			}
			if (UVerticalBoxSlot* RowSlot = Bars->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
			}
		};

		USizeBox* Box = nullptr;
		UProgressBar* Fill = nullptr;
		UProgressBar* Lag = nullptr;

		MakeBarRow(TEXT("Health"), HealthColor, true, Box, Fill, Lag);
		HealthBox = Box;
		HealthFill = Fill;
		HealthLag = Lag;
		HealthText = MakeText(Tree, 9, TextColor, TEXT("HealthText"));
		AddRow(HealthBox, HealthText);

		MakeBarRow(TEXT("Stamina"), StaminaColor, false, Box, Fill, Lag);
		StaminaBox = Box;
		StaminaFill = Fill;
		StaminaText = MakeText(Tree, 9, TextColor, TEXT("StaminaText"));
		AddRow(StaminaBox, StaminaText);
	}

	// ---- 왼쪽 아래: 장비 십자 슬롯 (위: 주문, 왼쪽: 왼손, 오른쪽: 오른손, 아래: 아이템) ----
	{
		UGridPanel* Grid = Tree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("EquipmentSlots"));
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Grid))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0, 1.0));
			CanvasSlot->SetPosition(FVector2D(60.0, -48.0));
			CanvasSlot->SetAutoSize(true);
		}

		auto AddSlot = [&](int32 Row, int32 Column, UTextBlock* Label)
		{
			USizeBox* Cell = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Cell->SetWidthOverride(76.0f);
			Cell->SetHeightOverride(76.0f);

			UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Frame->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
			Frame->SetPadding(FMargin(4.0f));
			Frame->SetHorizontalAlignment(HAlign_Center);
			Frame->SetVerticalAlignment(VAlign_Center);
			Cell->AddChild(Frame);
			if (Label)
			{
				Label->SetJustification(ETextJustify::Center);
				Label->SetAutoWrapText(true);
				Frame->SetContent(Label);
			}

			if (UGridSlot* GridSlot = Grid->AddChildToGrid(Cell, Row, Column))
			{
				GridSlot->SetPadding(FMargin(3.0f));
			}
		};

		RightHandText = MakeText(Tree, 10, TextColor, TEXT("RightHandText"));
		AddSlot(0, 1, nullptr);
		AddSlot(1, 0, nullptr);
		AddSlot(1, 2, RightHandText);

		// 아래 슬롯: 회복 물약 (다크소울 에스트 병처럼 이름과 남은 개수)
		{
			UVerticalBox* ItemBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemSlot"));
			ItemNameText = MakeText(Tree, 9, TextColor, TEXT("ItemNameText"));
			ItemNameText->SetText(NSLOCTEXT("OnlyDungeon", "HUDPotionName", "회복 물약"));
			ItemNameText->SetJustification(ETextJustify::Center);
			ItemCountText = MakeText(Tree, 18, FLinearColor(0.95f, 0.85f, 0.55f), TEXT("ItemCountText"));
			ItemCountText->SetJustification(ETextJustify::Center);
			if (UVerticalBoxSlot* NameSlot = ItemBox->AddChildToVerticalBox(ItemNameText))
			{
				NameSlot->SetHorizontalAlignment(HAlign_Center);
			}
			if (UVerticalBoxSlot* CountSlot = ItemBox->AddChildToVerticalBox(ItemCountText))
			{
				CountSlot->SetHorizontalAlignment(HAlign_Center);
			}

			USizeBox* Cell = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Cell->SetWidthOverride(76.0f);
			Cell->SetHeightOverride(76.0f);
			UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Frame->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
			Frame->SetPadding(FMargin(4.0f));
			Frame->SetHorizontalAlignment(HAlign_Center);
			Frame->SetVerticalAlignment(VAlign_Center);
			Frame->SetContent(ItemBox);
			Cell->AddChild(Frame);
			if (UGridSlot* GridSlot = Grid->AddChildToGrid(Cell, 2, 1))
			{
				GridSlot->SetPadding(FMargin(3.0f));
			}
		}
	}

	// ---- 아래 가운데: 대상 체력 (보스 바 형식) ----
	{
		UVerticalBox* Panel = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TargetPanel"));
		TargetPanel = Panel;
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Panel))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5, 1.0));
			CanvasSlot->SetPosition(FVector2D(0.0, -110.0));
			CanvasSlot->SetAutoSize(true);
		}

		TargetNameText = MakeText(Tree, 13, TextColor, TEXT("TargetNameText"));
		if (UVerticalBoxSlot* NameSlot = Panel->AddChildToVerticalBox(TargetNameText))
		{
			NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		USizeBox* Box = nullptr;
		UProgressBar* Fill = nullptr;
		UProgressBar* Lag = nullptr;
		MakeBarRow(TEXT("Target"), HealthColor, true, Box, Fill, Lag);
		Box->SetWidthOverride(TargetBarWidth);
		Box->SetHeightOverride(BarHeight * 0.85f);
		TargetFill = Fill;
		TargetLag = Lag;
		Panel->AddChildToVerticalBox(Box);
		Panel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- 가운데 위: 보스 이름, 가운데: 승리 문구 (평소엔 숨김) ----
	{
		BossTitleText = MakeText(Tree, 40, FLinearColor(0.92f, 0.86f, 0.72f), TEXT("BossTitleText"));
		BossTitleText->SetJustification(ETextJustify::Center);
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(BossTitleText))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.3f));
			CanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
			CanvasSlot->SetAutoSize(true);
		}
		BossTitleText->SetVisibility(ESlateVisibility::Collapsed);

		VictoryText = MakeText(Tree, 46, FLinearColor(0.95f, 0.78f, 0.35f), TEXT("VictoryText"));
		VictoryText->SetText(NSLOCTEXT("OnlyDungeon", "HUDVictory", "강적 격파"));
		VictoryText->SetJustification(ETextJustify::Center);
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(VictoryText))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.32f));
			CanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
			CanvasSlot->SetAutoSize(true);
		}
		VictoryText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- 가운데: 사망 문구 ----
	{
		DeathText = MakeText(Tree, 48, FLinearColor(0.6f, 0.05f, 0.04f), TEXT("DeathText"));
		DeathText->SetText(NSLOCTEXT("OnlyDungeon", "HUDDeath", "사망"));
		DeathText->SetJustification(ETextJustify::Center);
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(DeathText))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			CanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
			CanvasSlot->SetAutoSize(true);
		}
		DeathText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- 아래 가운데: 상호작용 안내 ----
	{
		UBorder* Prompt = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InteractionPrompt"));
		InteractionPrompt = Prompt;
		Prompt->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Prompt->SetPadding(FMargin(18.0f, 8.0f));
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Prompt))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5, 1.0));
			CanvasSlot->SetPosition(FVector2D(0.0, -200.0));
			CanvasSlot->SetAutoSize(true);
		}
		InteractionText = MakeText(Tree, 14, TextColor, TEXT("InteractionText"));
		Prompt->SetContent(InteractionText);
		Prompt->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- 락온 표식 (대상 위, 마름모) ----
	{
		UBorder* Marker = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LockOnMarker"));
		LockOnMarker = Marker;
		Marker->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.9f));
		Marker->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		Marker->SetRenderTransformAngle(45.0f);
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Marker))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
			CanvasSlot->SetSize(FVector2D(10.0, 10.0));
		}
		Marker->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ---- 오른쪽 위: 테스트 정보 ----
	{
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DebugPanel"));
		DebugPanel = Panel;
		Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
		Panel->SetPadding(FMargin(10.0f, 8.0f));
		if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Panel))
		{
			CanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(1.0, 0.0));
			CanvasSlot->SetPosition(FVector2D(-24.0, 24.0));
			CanvasSlot->SetAutoSize(true);
		}

		UVerticalBox* Lines = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Lines);
		DebugText = MakeText(Tree, 10, TextColor, TEXT("DebugText"));
		Lines->AddChildToVerticalBox(DebugText);
		FailureText = MakeText(Tree, 11, FLinearColor(1.0f, 0.35f, 0.3f), TEXT("FailureText"));
		if (UVerticalBoxSlot* FailSlot = Lines->AddChildToVerticalBox(FailureText))
		{
			FailSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}
		FailureText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UODHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// HUD는 입력을 가로채지 않는다.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DebugTimer, FTimerDelegate::CreateUObject(this, &ThisClass::RefreshDebugInfo), ODHUD::DebugTickInterval, true);

		// 보스전 시작·종료에 맞춰 보스 체력 바를 띄운다.
		if (UODBossEncounterSubsystem* Encounter = World->GetSubsystem<UODBossEncounterSubsystem>())
		{
			Encounter->OnEncounterStarted.AddUniqueDynamic(this, &ThisClass::HandleBossEncounterStarted);
			Encounter->OnEncounterEnded.AddUniqueDynamic(this, &ThisClass::HandleBossEncounterEnded);
			if (Encounter->IsEncounterActive())
			{
				HandleBossEncounterStarted(Encounter->GetCurrentBoss());
			}
		}
	}
}

void UODHUDWidget::HandleBossEncounterStarted(AODEnemyCharacter* Boss)
{
	if (!IsValid(Boss))
	{
		return;
	}

	bBossBarPinned = false;
	SetTarget(Boss);
	bBossBarPinned = true;

	// 보스 이름을 크게 띄운다. (입장 컷신이 끝나고 포효할 때)
	if (IsValid(BossTitleText))
	{
		BossTitleText->SetText(Boss->GetDisplayName());
		ShowBanner(BossTitleText, BossTitleStart, 0.3f);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TargetHideTimer);
	}
}

void UODHUDWidget::HandleBossEncounterEnded(bool bVictory)
{
	bBossBarPinned = false;
	if (bVictory)
	{
		// 느린 화면이 끝나갈 즈음 승리 문구
		ShowBanner(VictoryText, VictoryStart, 1.0f);
		// 빈 체력 바를 잠시 보여 준 뒤 숨긴다.
		StartTargetHideTimer();
	}
	else
	{
		ClearTarget();
	}
}

namespace ODHUDBanner
{
	constexpr float FadeIn = 0.6f;
	constexpr float Hold = 2.2f;
	constexpr float FadeOut = 1.0f;
}

void UODHUDWidget::ShowBanner(UTextBlock* Banner, double& OutStartTime, float Delay)
{
	UWorld* World = GetWorld();
	if (!IsValid(Banner) || !IsValid(World))
	{
		return;
	}
	OutStartTime = World->GetRealTimeSeconds() + Delay;
	Banner->SetRenderOpacity(0.0f);
	Banner->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!World->GetTimerManager().IsTimerActive(BannerTimer))
	{
		World->GetTimerManager().SetTimer(BannerTimer, this, &ThisClass::UpdateBanners, ODHUD::BannerTickInterval, true);
	}
}

float UODHUDWidget::GetBannerOpacity(double StartTime, double Now) const
{
	if (StartTime < 0.0)
	{
		return -1.0f;
	}
	const float T = static_cast<float>(Now - StartTime);
	if (T < 0.0f)
	{
		return 0.0f;
	}
	if (T < ODHUDBanner::FadeIn)
	{
		return T / ODHUDBanner::FadeIn;
	}
	if (T < ODHUDBanner::FadeIn + ODHUDBanner::Hold)
	{
		return 1.0f;
	}
	const float Out = T - ODHUDBanner::FadeIn - ODHUDBanner::Hold;
	return Out < ODHUDBanner::FadeOut ? 1.0f - Out / ODHUDBanner::FadeOut : -1.0f;
}

void UODHUDWidget::UpdateBanners()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	const double Now = World->GetRealTimeSeconds();
	bool bAnyActive = false;
	const auto Apply = [&](UTextBlock* Banner, double& StartTime)
	{
		if (!IsValid(Banner) || StartTime < 0.0)
		{
			return;
		}
		const float Opacity = GetBannerOpacity(StartTime, Now);
		if (Opacity < 0.0f)
		{
			Banner->SetVisibility(ESlateVisibility::Collapsed);
			StartTime = -1.0;
			return;
		}
		Banner->SetRenderOpacity(Opacity);
		bAnyActive = true;
	};
	Apply(BossTitleText, BossTitleStart);
	Apply(VictoryText, VictoryStart);
	if (!bAnyActive)
	{
		World->GetTimerManager().ClearTimer(BannerTimer);
	}
}

void UODHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(LagTimer);
		TimerManager.ClearTimer(DebugTimer);
		TimerManager.ClearTimer(TargetHideTimer);
		TimerManager.ClearTimer(FailureHideTimer);
		TimerManager.ClearTimer(LockOnMarkerTimer);
		TimerManager.ClearTimer(BannerTimer);

		if (UODBossEncounterSubsystem* Encounter = World->GetSubsystem<UODBossEncounterSubsystem>())
		{
			Encounter->OnEncounterStarted.RemoveDynamic(this, &ThisClass::HandleBossEncounterStarted);
			Encounter->OnEncounterEnded.RemoveDynamic(this, &ThisClass::HandleBossEncounterEnded);
		}
	}
	bBossBarPinned = false;
	ClearTarget();
	UnbindFromASC();
	UnbindPawnComponents();

	Super::NativeDestruct();
}

void UODHUDWidget::SetObservedPawn(APawn* NewPawn)
{
	if (ObservedPawn.Get() == NewPawn)
	{
		return;
	}

	UnbindFromASC();
	UnbindPawnComponents();
	ClearTarget();
	ObservedPawn = NewPawn;
	LastWeaponData.Reset();
	HealthState = FODHUDBarState();
	StaminaState = FODHUDBarState();

	if (IsValid(NewPawn))
	{
		BindToASC(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewPawn));
		BindPawnComponents(NewPawn);
	}
	RefreshDebugInfo();
}

void UODHUDWidget::BindPawnComponents(APawn* Pawn)
{
	if (UODCombatModeComponent* ModeComponent = Pawn->FindComponentByClass<UODCombatModeComponent>())
	{
		ObservedModeComponent = ModeComponent;
		ModeComponent->OnCinematicChanged.AddUniqueDynamic(this, &ThisClass::HandleCinematicChanged);
		HandleCinematicChanged(ModeComponent->IsInCinematic());
	}
	if (UODInteractionComponent* Interaction = Pawn->FindComponentByClass<UODInteractionComponent>())
	{
		ObservedInteraction = Interaction;
		Interaction->OnFocusChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionFocusChanged);
		HandleInteractionFocusChanged(Interaction->GetFocusedActor());
	}
	if (AODCharacterBase* Character = Cast<AODCharacterBase>(Pawn))
	{
		Character->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleObservedPawnDeath);
	}
	RefreshDeathText();
	if (UODPotionComponent* Potion = Pawn->FindComponentByClass<UODPotionComponent>())
	{
		ObservedPotion = Potion;
		Potion->OnChargesChanged.AddUniqueDynamic(this, &ThisClass::HandlePotionChargesChanged);
		HandlePotionChargesChanged(Potion->GetCharges(), Potion->GetMaxCharges());
	}
	if (UODLockOnComponent* LockOn = Pawn->FindComponentByClass<UODLockOnComponent>())
	{
		ObservedLockOn = LockOn;
		LockOn->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleLockOnTargetChanged);
		HandleLockOnTargetChanged(LockOn->GetTarget());
	}
}

void UODHUDWidget::HandleObservedPawnDeath(AODCharacterBase* DeadCharacter)
{
	RefreshDeathText();
}

void UODHUDWidget::RefreshDeathText()
{
	if (!IsValid(DeathText))
	{
		return;
	}
	const AODCharacterBase* Character = Cast<AODCharacterBase>(ObservedPawn.Get());
	const bool bDead = IsValid(Character) && Character->IsDead();
	DeathText->SetVisibility(bDead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UODHUDWidget::UnbindPawnComponents()
{
	if (AODCharacterBase* Character = Cast<AODCharacterBase>(ObservedPawn.Get()))
	{
		Character->OnDeath.RemoveDynamic(this, &ThisClass::HandleObservedPawnDeath);
	}
	if (UODCombatModeComponent* ModeComponent = ObservedModeComponent.Get())
	{
		ModeComponent->OnCinematicChanged.RemoveDynamic(this, &ThisClass::HandleCinematicChanged);
	}
	if (UODInteractionComponent* Interaction = ObservedInteraction.Get())
	{
		Interaction->OnFocusChanged.RemoveDynamic(this, &ThisClass::HandleInteractionFocusChanged);
	}
	if (UODLockOnComponent* LockOn = ObservedLockOn.Get())
	{
		LockOn->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleLockOnTargetChanged);
	}
	if (UODPotionComponent* Potion = ObservedPotion.Get())
	{
		Potion->OnChargesChanged.RemoveDynamic(this, &ThisClass::HandlePotionChargesChanged);
	}
	ObservedPotion.Reset();
	ObservedModeComponent.Reset();
	ObservedInteraction.Reset();
	ObservedLockOn.Reset();
	HandleLockOnTargetChanged(nullptr);
}

void UODHUDWidget::HandlePotionChargesChanged(int32 CurrentCharges, int32 MaxCharges)
{
	if (IsValid(ItemCountText))
	{
		ItemCountText->SetText(FText::AsNumber(CurrentCharges));
		// 다 쓰면 흐리게 (다크소울의 빈 에스트 병처럼)
		ItemCountText->SetColorAndOpacity(FSlateColor(CurrentCharges > 0 ? FLinearColor(0.95f, 0.85f, 0.55f) : FLinearColor(0.4f, 0.4f, 0.4f)));
	}
	if (IsValid(ItemNameText))
	{
		ItemNameText->SetColorAndOpacity(FSlateColor(CurrentCharges > 0 ? FLinearColor(0.85f, 0.82f, 0.75f) : FLinearColor(0.4f, 0.4f, 0.4f)));
	}
}

void UODHUDWidget::HandleLockOnTargetChanged(AActor* NewTarget)
{
	UWorld* World = GetWorld();
	const bool bLockedOn = IsValid(NewTarget);

	// 하단 대상 바는 보스 전용이라 락온은 표식만 띄운다. (일반 몬스터 체력은 머리 위 이름표)
	if (bLockedOn)
	{
		if (World)
		{
			World->GetTimerManager().SetTimer(LockOnMarkerTimer, FTimerDelegate::CreateUObject(this, &ThisClass::UpdateLockOnMarker), ODHUD::LockOnMarkerInterval, true);
		}
		UpdateLockOnMarker();
		return;
	}

	if (World)
	{
		World->GetTimerManager().ClearTimer(LockOnMarkerTimer);
	}
	if (IsValid(LockOnMarker))
	{
		LockOnMarker->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UODHUDWidget::UpdateLockOnMarker()
{
	if (!IsValid(LockOnMarker))
	{
		return;
	}

	const UODLockOnComponent* LockOn = ObservedLockOn.Get();
	const APawn* Pawn = ObservedPawn.Get();
	APlayerController* PC = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(LockOnMarker->Slot);

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	const bool bVisible = LockOn && LockOn->IsLockedOn() && PC && MarkerSlot
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, LockOn->GetTargetAimLocation(), ScreenPosition, false);
	if (bVisible)
	{
		MarkerSlot->SetPosition(ScreenPosition);
	}
	LockOnMarker->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UODHUDWidget::StartTargetHideTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TargetHideTimer, FTimerDelegate::CreateUObject(this, &ThisClass::ClearTarget), TargetHideDelay, false);
	}
}

void UODHUDWidget::HandleCinematicChanged(bool bInCinematic)
{
	// 다크소울처럼 컷신 중에는 HUD를 모두 숨긴다.
	SetVisibility(bInCinematic ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UODHUDWidget::HandleInteractionFocusChanged(AActor* NewFocus)
{
	const UODInteractionComponent* Interaction = ObservedInteraction.Get();
	const FText Label = (IsValid(NewFocus) && Interaction) ? Interaction->GetFocusedInteractionText() : FText::GetEmpty();

	if (IsValid(InteractionText))
	{
		InteractionText->SetText(FText::Format(NSLOCTEXT("OnlyDungeon", "InteractionPromptFormat", "{0}    {1}"), InteractionKeyLabel, Label));
	}
	if (IsValid(InteractionPrompt))
	{
		InteractionPrompt->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UODHUDWidget::BindToASC(UAbilitySystemComponent* ASC)
{
	if (!IsValid(ASC))
	{
		return;
	}
	ObservedASC = ASC;

	const FGameplayAttribute PlayerAttributes[] = {
		UODAttributeSet_Health::GetHealthAttribute(),
		UODAttributeSet_Health::GetMaxHealthAttribute(),
		UODAttributeSet_Combat::GetStaminaAttribute(),
		UODAttributeSet_Combat::GetMaxStaminaAttribute(),
	};
	PlayerAttributeHandles.Reserve(UE_ARRAY_COUNT(PlayerAttributes));
	for (const FGameplayAttribute& Attribute : PlayerAttributes)
	{
		const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddWeakLambda(this, [this](const FOnAttributeChangeData&)
		{
			RefreshPlayerBars();
		});
		PlayerAttributeHandles.Emplace(Attribute, Handle);
	}

	if (UODAbilitySystemComponent* ODASC = Cast<UODAbilitySystemComponent>(ASC))
	{
		FailureHandle = ODASC->OnAbilityActivationFailed.AddUObject(this, &ThisClass::HandleActivationFailed);
	}

	RefreshPlayerBars();
}

void UODHUDWidget::UnbindFromASC()
{
	if (UAbilitySystemComponent* ASC = ObservedASC.Get(); IsValid(ASC))
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : PlayerAttributeHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
		if (UODAbilitySystemComponent* ODASC = Cast<UODAbilitySystemComponent>(ASC))
		{
			ODASC->OnAbilityActivationFailed.Remove(FailureHandle);
		}
	}
	PlayerAttributeHandles.Reset();
	FailureHandle.Reset();
	ObservedASC.Reset();
}

void UODHUDWidget::RefreshPlayerBars()
{
	const UAbilitySystemComponent* ASC = ObservedASC.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	ApplyBar(HealthState,
		ASC->GetNumericAttribute(UODAttributeSet_Health::GetHealthAttribute()),
		ASC->GetNumericAttribute(UODAttributeSet_Health::GetMaxHealthAttribute()),
		HealthFill, HealthLag, HealthBox, HealthText, true);

	ApplyBar(StaminaState,
		ASC->GetNumericAttribute(UODAttributeSet_Combat::GetStaminaAttribute()),
		ASC->GetNumericAttribute(UODAttributeSet_Combat::GetMaxStaminaAttribute()),
		StaminaFill, nullptr, StaminaBox, StaminaText, false);

	// 부활하면 초기 수치 GE로 체력이 바뀌므로 여기서 사망 문구를 내린다.
	RefreshDeathText();
}

void UODHUDWidget::ApplyBar(FODHUDBarState& State, float NewCurrent, float NewMax, UProgressBar* Fill, UProgressBar* Lag, USizeBox* Box, UTextBlock* ValueText, bool bUseLag)
{
	NewMax = FMath::Max(NewMax, 1.0f);
	NewCurrent = FMath::Clamp(NewCurrent, 0.0f, NewMax);

	if (!State.bInitialized || !bUseLag)
	{
		State.Lag = NewCurrent;
	}
	else if (NewCurrent < State.Current)
	{
		// 깎인 만큼 지연 바로 남겨 두고, 잠시 뒤부터 줄인다.
		State.Lag = FMath::Max(State.Lag, State.Current);
		const UWorld* World = GetWorld();
		State.LagHoldUntil = (World ? World->GetTimeSeconds() : 0.0) + LagDelay;
		StartLagTimer();
	}
	else if (NewCurrent > State.Lag)
	{
		State.Lag = NewCurrent;
	}

	State.Current = NewCurrent;
	State.Max = NewMax;
	State.bInitialized = true;

	if (IsValid(Fill))
	{
		Fill->SetPercent(State.Current / State.Max);
	}
	if (IsValid(Lag))
	{
		Lag->SetPercent(State.Lag / State.Max);
	}
	if (IsValid(Box))
	{
		// 플레이어 바는 최대치에 비례한 길이 (대상 바는 Box를 넘기지 않아 길이 고정)
		Box->SetWidthOverride(FMath::Clamp(State.Max * PixelsPerPoint, BarWidthRange.X, BarWidthRange.Y));
	}
	if (IsValid(ValueText))
	{
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), State.Current, State.Max)));
	}
}

void UODHUDWidget::StartLagTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(LagTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(LagTimer, FTimerDelegate::CreateUObject(this, &ThisClass::UpdateLag), ODHUD::LagTickInterval, true);
}

void UODHUDWidget::UpdateLag()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float Step = ODHUD::LagTickInterval * LagSpeed;

	bool bPending = false;
	auto Advance = [&](FODHUDBarState& State, UProgressBar* Lag)
	{
		if (State.Lag <= State.Current)
		{
			return;
		}
		bPending = true;
		if (Now < State.LagHoldUntil)
		{
			return;
		}
		State.Lag = FMath::Max(State.Current, State.Lag - State.Max * Step);
		if (IsValid(Lag))
		{
			Lag->SetPercent(State.Lag / State.Max);
		}
	};
	Advance(HealthState, HealthLag);
	Advance(TargetState, TargetLag);

	if (!bPending)
	{
		World->GetTimerManager().ClearTimer(LagTimer);
	}
}

void UODHUDWidget::SetTarget(AActor* NewTarget)
{
	// 보스전 중에는 대상 바가 보스에 고정된다.
	if (bBossBarPinned || Target.Get() == NewTarget)
	{
		return;
	}

	ClearTarget();

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewTarget);
	if (!IsValid(ASC))
	{
		return;
	}
	Target = NewTarget;
	TargetASC = ASC;
	TargetState = FODHUDBarState();

	const FGameplayAttribute TargetAttributes[] = {
		UODAttributeSet_Health::GetHealthAttribute(),
		UODAttributeSet_Health::GetMaxHealthAttribute(),
	};
	TargetAttributeHandles.Reserve(UE_ARRAY_COUNT(TargetAttributes));
	for (const FGameplayAttribute& Attribute : TargetAttributes)
	{
		const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddWeakLambda(this, [this](const FOnAttributeChangeData&)
		{
			RefreshTargetBar();
		});
		TargetAttributeHandles.Emplace(Attribute, Handle);
	}

	if (IsValid(TargetNameText))
	{
		const AODCharacterBase* TargetCharacter = Cast<AODCharacterBase>(NewTarget);
		TargetNameText->SetText(TargetCharacter ? TargetCharacter->GetDisplayName() : FText::FromString(NewTarget->GetName()));
	}
	if (IsValid(TargetPanel))
	{
		TargetPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	RefreshTargetBar();
}

void UODHUDWidget::ClearTarget()
{
	if (UAbilitySystemComponent* ASC = TargetASC.Get(); IsValid(ASC))
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : TargetAttributeHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}
	TargetAttributeHandles.Reset();
	TargetASC.Reset();
	Target.Reset();
	TargetState = FODHUDBarState();

	if (IsValid(TargetPanel))
	{
		TargetPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UODHUDWidget::RefreshTargetBar()
{
	const UAbilitySystemComponent* ASC = TargetASC.Get();
	if (!IsValid(ASC))
	{
		return;
	}
	ApplyBar(TargetState,
		ASC->GetNumericAttribute(UODAttributeSet_Health::GetHealthAttribute()),
		ASC->GetNumericAttribute(UODAttributeSet_Health::GetMaxHealthAttribute()),
		TargetFill, TargetLag, nullptr, nullptr, true);
}

void UODHUDWidget::HandleActivationFailed(const UGameplayAbility* Ability, const FString& Reason)
{
	if (!IsValid(FailureText))
	{
		return;
	}

	FString AbilityName = GetNameSafe(Ability);
	AbilityName.RemoveFromStart(TEXT("Default__"));
	AbilityName.RemoveFromEnd(TEXT("_C"));
	FailureText->SetText(FText::FromString(FString::Printf(TEXT("%s  (%s)"), *Reason, *AbilityName)));
	FailureText->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FailureHideTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsValid(FailureText))
			{
				FailureText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}), FailureShowTime, false);
	}
}

void UODHUDWidget::RefreshDebugInfo()
{
	const APawn* Pawn = ObservedPawn.Get();
	const AODCharacterBase* Character = Cast<AODCharacterBase>(Pawn);

	// 오른손 슬롯: 무기가 바뀌었을 때만 글자를 바꾼다.
	const UODWeaponData* WeaponData = (IsValid(Character) && IsValid(Character->GetEquipmentComponent())) ? Character->GetEquipmentComponent()->GetWeaponData() : nullptr;
	if (LastWeaponData.Get() != WeaponData)
	{
		LastWeaponData = WeaponData;
		if (IsValid(RightHandText))
		{
			FText WeaponName = FText::GetEmpty();
			if (IsValid(WeaponData))
			{
				WeaponName = !WeaponData->DisplayName.IsEmpty() ? WeaponData->DisplayName : FText::FromString(WeaponData->GetName());
			}
			RightHandText->SetText(WeaponName);
		}
	}

	const bool bShowDebug = ODHUD::CVarShowDebug.GetValueOnGameThread();
	if (IsValid(DebugPanel))
	{
		const ESlateVisibility Wanted = bShowDebug ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (DebugPanel->GetVisibility() != Wanted)
		{
			DebugPanel->SetVisibility(Wanted);
		}
	}
	if (!bShowDebug || !IsValid(DebugText))
	{
		return;
	}

	const UAbilitySystemComponent* ASC = ObservedASC.Get();
	TArray<FString> Lines;
	Lines.Reserve(5);

	// 전투 모드
	if (const UODCombatModeComponent* ModeComponent = ObservedModeComponent.Get())
	{
		Lines.Emplace(FString::Printf(TEXT("모드: %s%s"),
			ModeComponent->GetCombatMode() == EODCombatMode::Field ? TEXT("필드(쿼터뷰)") : TEXT("보스(3인칭)"),
			ModeComponent->IsInCinematic() ? TEXT(" · 컷신") : TEXT("")));
	}

	// 상태 태그
	TArray<FString> States;
	if (IsValid(ASC))
	{
		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);
		const FGameplayTag StateRoot = FGameplayTag::RequestGameplayTag(TEXT("State"), false);
		for (const FGameplayTag& Tag : OwnedTags)
		{
			if (StateRoot.IsValid() && Tag.MatchesTag(StateRoot) && Tag != StateRoot)
			{
				States.Emplace(ODHUD::StateLabel(Tag));
			}
		}
	}
	Lines.Emplace(FString::Printf(TEXT("상태: %s"), States.IsEmpty() ? TEXT("-") : *FString::Join(States, TEXT(", "))));

	// 몽타주·섹션
	FString MontageInfo = TEXT("-");
	if (const ACharacter* AsCharacter = Cast<ACharacter>(Pawn); IsValid(AsCharacter) && IsValid(AsCharacter->GetMesh()))
	{
		if (UAnimInstance* AnimInstance = AsCharacter->GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* Montage = AnimInstance->GetCurrentActiveMontage())
			{
				MontageInfo = FString::Printf(TEXT("%s / %s (%.2f)"), *Montage->GetName(), *AnimInstance->Montage_GetCurrentSection(Montage).ToString(), AnimInstance->Montage_GetPosition(Montage));
			}
		}
	}
	Lines.Emplace(FString::Printf(TEXT("몽타주: %s"), *MontageInfo));

	// 강인도
	if (IsValid(ASC))
	{
		Lines.Emplace(FString::Printf(TEXT("강인도: %.0f / %.0f"),
			ASC->GetNumericAttribute(UODAttributeSet_Health::GetPoiseAttribute()),
			ASC->GetNumericAttribute(UODAttributeSet_Health::GetMaxPoiseAttribute())));
	}

	DebugText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}
