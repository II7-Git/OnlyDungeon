// OnlyDungeon 일시정지 메뉴

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ODPauseMenuWidget.generated.h"

class USlider;

/**
 * 일시정지 메뉴: 계속하기 / 볼륨(배경음·효과음·환경음) / 타이틀로 / 게임 종료.
 * 볼륨은 움직이는 즉시 소리에 반영하고, 메뉴를 닫을 때 설정 파일에 쓴다.
 */
UCLASS()
class ONLYDUNGEON_API UODPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildDefaultLayout();

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleTitleClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleMusicChanged(float Value);

	UFUNCTION()
	void HandleSFXChanged(float Value);

	UFUNCTION()
	void HandleAmbienceChanged(float Value);

	UPROPERTY()
	TObjectPtr<USlider> MusicSlider;

	UPROPERTY()
	TObjectPtr<USlider> SFXSlider;

	UPROPERTY()
	TObjectPtr<USlider> AmbienceSlider;
};
