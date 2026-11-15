// OnlyDungeon HUD 액터: 플레이어 HUD 위젯을 만들고 조종 중인 폰을 연결한다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ODHUD.generated.h"

class UODHUDWidget;

/**
 * 게임 모드의 HUD 클래스로 지정한다. (BP_ODHUD → BP_ODGameMode.HUDClass)
 * BeginPlay에서 HUDWidgetClass 위젯을 뷰포트에 띄우고, 폰이 바뀌면 위젯에 새 폰을 넘긴다.
 * 화면 그리기는 위젯이 담당하므로 DrawHUD는 쓰지 않는다.
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

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UPROPERTY(Transient)
	TObjectPtr<UODHUDWidget> HUDWidget;
};
