// OnlyDungeon 타이틀 화면

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ODTitleWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * 타이틀 화면: 새 게임 / 이어하기 / 종료.
 * 게임 모드가 타이틀 상태일 때 HUD가 띄운다. 이어할 저장이 없으면 이어하기 버튼이 꺼진다.
 * 저장이 있는데 새 게임을 누르면 덮어쓸지 한 번 더 묻는다.
 * BP로 상속해 디자이너에 같은 이름의 위젯을 두지 않아도, 비어 있으면 C++이 기본 레이아웃을 만든다.
 */
UCLASS()
class ONLYDUNGEON_API UODTitleWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

private:
	void BuildDefaultLayout();

	/** 이어하기 가능 여부에 맞춰 버튼을 다시 맞추고 확인 단계를 닫는다. */
	void RefreshButtons();
	void SetConfirmVisible(bool bVisible);

	UFUNCTION()
	void HandleNewGameClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleConfirmNewGameClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UPROPERTY()
	TObjectPtr<UVerticalBox> MainButtons;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ConfirmButtons;

	UPROPERTY()
	TObjectPtr<UButton> ContinueButton;
};
