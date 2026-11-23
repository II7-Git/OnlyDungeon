// OnlyDungeon 배경음·환경음 세트

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ODAudioSet.generated.h"

class USoundBase;

/**
 * 레벨에서 쓸 배경음·환경음 묶음. AODGameMode가 소프트 참조로 들고, UODAudioDirectorSubsystem이 레벨 시작 때 비동기로 읽는다.
 * 음악 파일은 크므로 소프트 참조로 두어, 이 세트를 쓰는 레벨에서만 메모리에 올라온다.
 * 반복 재생은 사운드 에셋 쪽(SoundWave의 Looping, SoundCue의 Looping 노드)에서 정한다.
 */
UCLASS(BlueprintType)
class ONLYDUNGEON_API UODAudioSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 필드(숲길) 배경음. 레벨 시작·부활 때 재생한다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Audio|Music")
	TSoftObjectPtr<USoundBase> FieldMusic;

	/** 보스전 배경음. 보스전이 시작되면 필드 배경음에서 크로스페이드한다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Audio|Music")
	TSoftObjectPtr<USoundBase> BossMusic;

	/** 보스 처치 순간 한 번 재생하는 효과음. 이후 배경음은 멈춘 채 환경음만 남는다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Audio|Music")
	TSoftObjectPtr<USoundBase> VictoryStinger;

	/** 플레이어 사망 순간 한 번 재생하는 효과음 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Audio|Music")
	TSoftObjectPtr<USoundBase> DeathStinger;

	/** 필드 환경음(숲 바람·새소리 등). 보스전 동안에는 줄였다가 끝나면 되돌린다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Audio|Ambience")
	TSoftObjectPtr<USoundBase> FieldAmbience;

	/** 배경음이 바뀔 때 겹쳐 넘어가는 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Audio", meta = (ClampMin = "0.0", Units = "s"))
	float CrossfadeTime = 2.0f;

	/** 사망·승리 때 배경음이 사라지는 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Audio", meta = (ClampMin = "0.0", Units = "s"))
	float StopFadeTime = 1.5f;

	/** 보스전 동안 환경음 크기 (0이면 끔) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Audio|Ambience", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BossAmbienceVolume = 0.0f;
};
