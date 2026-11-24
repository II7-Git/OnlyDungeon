// OnlyDungeon 저장 데이터

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ODSaveGame.generated.h"

class UODWeaponData;

/**
 * 게임 진행 저장 (슬롯 "OD_Progress"). 휴식·보스 처치·사망·메뉴로 나갈 때 게임 모드가 채워 저장한다.
 * 레벨에 배치된 액터(보스, 휴식 지점)는 레벨 안에서 바뀌지 않는 액터 이름(FName)으로 가리킨다.
 * 구조를 바꾸면 SaveVersion을 올린다. 버전이 다른 저장은 "저장 없음"으로 취급한다.
 */
UCLASS()
class ONLYDUNGEON_API UODSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** 현재 저장 구조 버전 */
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(SaveGame)
	int32 SaveVersion = CurrentVersion;

	/** 처치한 보스의 레벨 액터 이름 */
	UPROPERTY(SaveGame)
	TArray<FName> DefeatedBossNames;

	/** 마지막으로 휴식한 리스폰 포인트의 레벨 액터 이름 (없으면 레벨 시작 위치) */
	UPROPERTY(SaveGame)
	FName LastRespawnPointName;

	/** 장착 중이던 무기 */
	UPROPERTY(SaveGame)
	TSoftObjectPtr<UODWeaponData> EquippedWeapon;

	UPROPERTY(SaveGame)
	int32 DeathCount = 0;

	/** 누적 플레이 시간(초, 일시정지·메뉴 시간 제외) */
	UPROPERTY(SaveGame)
	double PlayTimeSeconds = 0.0;

	/** 보스를 처치해 클리어 화면을 본 적이 있는지 */
	UPROPERTY(SaveGame)
	bool bGameCleared = false;
};

/** 설정 저장 (슬롯 "OD_Settings"). 새 게임을 해도 지우지 않는다. */
UCLASS()
class ONLYDUNGEON_API UODSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** 볼륨 배율 (0~1). 사운드 클래스의 기본 볼륨에 곱한다. */
	UPROPERTY(SaveGame)
	float MusicVolume = 1.0f;

	UPROPERTY(SaveGame)
	float SFXVolume = 1.0f;

	UPROPERTY(SaveGame)
	float AmbienceVolume = 1.0f;
};
