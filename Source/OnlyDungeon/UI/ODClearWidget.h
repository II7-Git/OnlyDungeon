// OnlyDungeon 클리어 화면

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ODClearWidget.generated.h"

class UTextBlock;

/**
 * 보스를 처치하면(첫 클리어) 승리 연출 뒤에 뜨는 화면: 플레이 시간·사망 횟수, 계속 탐험 / 타이틀로.
 * 띄울 때마다 게임 모드의 기록을 읽어 다시 쓴다.
 */
UCLASS()
class ONLYDUNGEON_API UODClearWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

private:
	void BuildDefaultLayout();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleTitleClicked();

	UPROPERTY()
	TObjectPtr<UTextBlock> StatsText;
};
