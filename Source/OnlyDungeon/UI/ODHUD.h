// OnlyDungeon HUD 액터: 플레이어 HUD 위젯을 만들고 조종 중인 폰을 연결한다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/ODGameMode.h"
#include "ODHUD.generated.h"

class UODClearWidget;
class UODHUDWidget;
class UODPauseMenuWidget;
class UODTitleWidget;
class UUserWidget;

/**
 * 게임 모드의 HUD 클래스로 지정한다. (BP_ODHUD → BP_ODGameMode.HUDClass)
 * BeginPlay에서 HUDWidgetClass 위젯을 뷰포트에 띄우고, 폰이 바뀌면 위젯에 새 폰을 넘긴다.
 * 화면 그리기는 위젯이 담당하므로 DrawHUD는 쓰지 않는다.
 * 게임 모드의 흐름(타이틀·플레이·클리어)과 일시정지 알림을 구독해 메뉴 화면을 띄우고 내린다.
 * 메뉴가 떠 있는 동안 커서를 보이고 UI 입력을 받으며(일시정지 키는 계속 받음), 닫히면 전투 모드의 입력 설정으로 되돌린다.
 */
UCLASS()
class ONLYDUNGEON_API AODHUD : public AHUD
{
	GENERATED_BODY()

public:
	AODHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UODHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 띄울 HUD 위젯 (WBP_OD_HUD) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD")
	TSubclassOf<UODHUDWidget> HUDWidgetClass;

	/** 메뉴 위젯 (비우면 C++ 기본 레이아웃) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Menu")
	TSubclassOf<UODTitleWidget> TitleWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Menu")
	TSubclassOf<UODPauseMenuWidget> PauseMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Menu")
	TSubclassOf<UODClearWidget> ClearWidgetClass;

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleGameFlowChanged(EODGameFlowState NewState);

	UFUNCTION()
	void HandleMenuPauseChanged(bool bPaused);

	/** 메뉴 위젯을 (처음이면 만들어) 띄우거나 내린다. */
	void SetMenuVisible(UUserWidget*& Widget, UClass* WidgetClass, bool bVisible);

	/** 떠 있는 메뉴에 맞춰 커서·입력 방식·HUD 표시를 맞춘다. */
	void RefreshInputMode();

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TitleWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> PauseMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ClearWidget;

	EODGameFlowState CurrentFlow = EODGameFlowState::Playing;
	bool bPauseMenuOpen = false;

	UPROPERTY(Transient)
	TObjectPtr<UODHUDWidget> HUDWidget;
};
