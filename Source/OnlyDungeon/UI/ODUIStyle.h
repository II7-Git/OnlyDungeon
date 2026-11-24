// OnlyDungeon UI 공통 스타일 (메뉴 위젯이 C++로 레이아웃을 만들 때 쓰는 도우미)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

namespace ODUIStyle
{
	/** 기본 글자색 (바랜 양피지색) */
	inline const FLinearColor TextColor(0.85f, 0.82f, 0.75f);
	/** 강조 글자색 (금빛) */
	inline const FLinearColor AccentColor(0.95f, 0.78f, 0.42f);
	/** 화면 전체를 덮는 어두운 막 */
	inline const FLinearColor DimColor(0.0f, 0.0f, 0.0f, 0.72f);
	/** 버튼 바탕 */
	inline const FLinearColor ButtonColor(0.06f, 0.05f, 0.04f, 0.85f);
	inline const FLinearColor ButtonHoverColor(0.22f, 0.16f, 0.08f, 0.95f);

	inline FSlateFontInfo Font(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle("Regular", Size);
	}

	inline UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 FontSize, const FLinearColor& Color = TextColor)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(Text);
		Block->SetFont(Font(FontSize));
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetShadowOffset(FVector2D(1.0, 1.0));
		Block->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		Block->SetJustification(ETextJustify::Center);
		return Block;
	}

	/** 자식을 최소 너비 상자에 넣어 돌려준다. (메뉴 버튼 너비를 맞추는 데 쓴다) */
	inline UWidget* WithMinWidth(UWidgetTree* Tree, UWidget* Child, float MinWidth)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetMinDesiredWidth(MinWidth);
		Box->AddChild(Child);
		return Box;
	}

	/** 글자가 들어간 메뉴 버튼. 클릭 연결은 호출한 쪽에서 OnClicked에 한다. */
	inline UButton* MakeButton(UWidgetTree* Tree, const FText& Label, UTextBlock** OutLabel = nullptr)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(ButtonColor);
		Style.Hovered.TintColor = FSlateColor(ButtonHoverColor);
		Style.Pressed.TintColor = FSlateColor(ButtonHoverColor);
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.03f, 0.03f, 0.03f, 0.6f));
		Button->SetStyle(Style);
		UTextBlock* Text = MakeText(Tree, Label, 22);
		Button->AddChild(Text);
		if (OutLabel)
		{
			*OutLabel = Text;
		}
		return Button;
	}
}
