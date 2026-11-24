// OnlyDungeon 저장 서브시스템

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ODSaveSubsystem.generated.h"

class UODSaveGame;
class UODSettingsSave;

/** 볼륨 채널 (사운드 클래스 단위) */
UENUM(BlueprintType)
enum class EODVolumeChannel : uint8
{
	Music,
	SFX,
	Ambience
};

/**
 * 진행 저장과 설정 저장을 관리한다. 게임 인스턴스에 있어 타이틀로 돌아가 레벨을 다시 열어도 유지된다.
 * - 진행 저장은 게임 모드가 채운 UODSaveGame을 비동기로 쓴다. (게임이 멈추지 않음) 종료 직전에만 동기로 쓴다.
 * - 저장 파일이 없거나, 깨졌거나, 버전이 다르면 HasProgress()가 false다. (이어하기 버튼이 꺼진다)
 */
UCLASS()
class ONLYDUNGEON_API UODSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 이어할 수 있는 진행 저장이 있는지 */
	bool HasProgress() const;

	/** 진행 저장을 읽어 돌려준다. 없거나 쓸 수 없으면 nullptr */
	UODSaveGame* LoadProgress();

	/** 진행 저장을 지우고 새 진행 데이터를 만든다. (새 게임) */
	UODSaveGame* CreateNewProgress();

	/** 현재 진행 데이터 (CreateNewProgress/LoadProgress 이후 유효) */
	UODSaveGame* GetProgress() const { return Progress; }

	/** 현재 진행 데이터를 저장한다. bSync는 게임 종료 직전처럼 완료를 기다려야 할 때만 쓴다. */
	void SaveProgress(bool bSync = false);

	float GetVolume(EODVolumeChannel Channel) const;

	/** 볼륨을 바꾸고 바로 소리에 적용한다. (파일에는 SaveSettings에서 쓴다) */
	void SetVolume(EODVolumeChannel Channel, float Volume);

	void SaveSettings();

	/** 진행 저장 슬롯 이름 */
	static const FString ProgressSlot;
	static const FString SettingsSlot;

private:
	/** 현재 월드의 배경음 서브시스템에 볼륨을 적용한다. */
	void ApplyVolumes() const;

	UPROPERTY()
	TObjectPtr<UODSaveGame> Progress;

	UPROPERTY()
	TObjectPtr<UODSettingsSave> Settings;

	/** 비동기 저장 진행 중 (연달아 저장 요청이 오면 끝난 뒤 한 번 더 쓴다) */
	bool bSaving = false;
	bool bSaveQueued = false;
};
