// OnlyDungeon 일반 몬스터 머리 위 이름·체력 표시

#include "ODEnemyNameplateWidget.h"

#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "Character/ODCharacterBase.h"

bool UODEnemyNameplateWidget::Initialize()
{
	const bool bSuperInitialized = Super::Initialize();
	if (bSuperInitialized && WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bSuperInitialized;
}

void UODEnemyNameplateWidget::BuildDefaultLayout()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 9));
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.82f, 0.75f)));
	NameText->SetShadowOffset(FVector2D(1.0, 1.0));
	NameText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	NameText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* NameSlot = Root->AddChildToVerticalBox(NameText))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	}

	BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarBox"));
	BarBox->SetWidthOverride(BarSize.X);
	BarBox->SetHeightOverride(BarSize.Y);
	if (UVerticalBoxSlot* BarSlot = Root->AddChildToVerticalBox(BarBox))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	BarBox->SetContent(Overlay);

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(FrameColor);
	if (UOverlaySlot* FrameSlot = Overlay->AddChildToOverlay(Frame))
	{
		FrameSlot->SetHorizontalAlignment(HAlign_Fill);
		FrameSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// 배경 없는 단색 진행 바 (프레임은 뒤의 Border가 그린다)
	HealthFill = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthFill"));
	FProgressBarStyle Style = HealthFill->GetWidgetStyle();
	Style.SetBackgroundImage(FSlateColorBrush(FLinearColor::Transparent));
	Style.SetFillImage(FSlateColorBrush(FLinearColor::White));
	HealthFill->SetWidgetStyle(Style);
	HealthFill->SetFillColorAndOpacity(HealthColor);
	HealthFill->SetPercent(1.0f);
	if (UOverlaySlot* FillSlot = Overlay->AddChildToOverlay(HealthFill))
	{
		FillSlot->SetHorizontalAlignment(HAlign_Fill);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
		FillSlot->SetPadding(FMargin(1.0f));
	}
}

void UODEnemyNameplateWidget::SetCharacter(AODCharacterBase* InCharacter)
{
	ObservedCharacter = InCharacter;
	Bind();
}

void UODEnemyNameplateWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 위젯 컴포넌트는 이름표를 숨길 때 위젯을 화면에서 내리고(NativeDestruct) 다시 보일 때 올린다(NativeConstruct).
	// 내려가 있는 동안 끊긴 체력 이벤트를 다시 연결하고, 그 사이 바뀐 체력도 반영한다.
	Bind();
}

void UODEnemyNameplateWidget::Bind()
{
	Unbind();
	AODCharacterBase* InCharacter = ObservedCharacter.Get();
	if (!IsValid(InCharacter))
	{
		return;
	}

	if (IsValid(NameText))
	{
		NameText->SetText(InCharacter->GetDisplayName());
	}

	UAbilitySystemComponent* ASC = InCharacter->GetAbilitySystemComponent();
	if (!IsValid(ASC))
	{
		return;
	}
	ObservedASC = ASC;

	const FGameplayAttribute Attributes[] = {
		UODAttributeSet_Health::GetHealthAttribute(),
		UODAttributeSet_Health::GetMaxHealthAttribute(),
	};
	AttributeHandles.Reserve(UE_ARRAY_COUNT(Attributes));
	for (const FGameplayAttribute& Attribute : Attributes)
	{
		const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddWeakLambda(this, [this](const FOnAttributeChangeData&)
		{
			RefreshHealth();
		});
		AttributeHandles.Emplace(Attribute, Handle);
	}
	RefreshHealth();
}

void UODEnemyNameplateWidget::RefreshHealth()
{
	const UAbilitySystemComponent* ASC = ObservedASC.Get();
	if (!IsValid(ASC) || !IsValid(HealthFill))
	{
		return;
	}
	const float MaxHealth = FMath::Max(ASC->GetNumericAttribute(UODAttributeSet_Health::GetMaxHealthAttribute()), 1.0f);
	HealthFill->SetPercent(FMath::Clamp(ASC->GetNumericAttribute(UODAttributeSet_Health::GetHealthAttribute()) / MaxHealth, 0.0f, 1.0f));
}

void UODEnemyNameplateWidget::Unbind()
{
	if (UAbilitySystemComponent* ASC = ObservedASC.Get(); IsValid(ASC))
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : AttributeHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}
	AttributeHandles.Reset();
	ObservedASC.Reset();
}

void UODEnemyNameplateWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}
