// OnlyDungeon 배경음·환경음 진행 서브시스템

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/StreamableManager.h"
#include "ODAudioDirectorSubsystem.generated.h"

class AODEnemyCharacter;
class UAudioComponent;
class UODAudioSet;
class USoundBase;

/**
 * 게임 흐름에 맞춰 배경음·환경음을 바꾼다. (Tick 없음, 알림으로만 동작)
 * - 레벨 시작: 게임 모드의 오디오 세트를 비동기로 읽은 뒤 필드 배경음·환경음을 시작한다.
 * - 보스전 시작/승리: UODBossEncounterSubsystem의 알림을 구독해 보스 배경음으로 넘어가거나, 승리 효과음 후 배경음을 멈춘다.
 * - 플레이어 사망/부활: 게임 모드가 알려 준다. 사망하면 배경음을 줄이고, 부활하면 필드 배경음으로 다시 시작한다.
 * 배경음은 두 오디오 컴포넌트를 번갈아 써서 크로스페이드한다. 같은 곡을 다시 요청하면 무시한다.
 */
UCLASS()
class ONLYDUNGEON_API UODAudioDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** 플레이어가 죽었다. 배경음을 줄이고 사망 효과음을 낸다. (게임 모드가 호출) */
	void NotifyPlayerDied();

	/** 플레이어가 부활했다. 필드 배경음과 환경음으로 돌아간다. (게임 모드가 호출) */
	void NotifyPlayerRevived();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandleBossEncounterStarted(AODEnemyCharacter* Boss);

	UFUNCTION()
	void HandleBossEncounterEnded(bool bVictory);

	/** 오디오 세트와 그 안의 사운드 로드가 끝났다. */
	void HandleAudioSetLoaded();

	/** 배경음을 Track으로 바꾼다. nullptr이면 페이드아웃만 한다. */
	void CrossfadeTo(USoundBase* Track, float FadeTime);

	/** 환경음 크기를 바꾼다. (필요하면 처음 재생) */
	void SetAmbienceVolume(float Volume, float FadeTime);

	void PlayStinger(USoundBase* Sound) const;

	/** 배경음·환경음용 2D 오디오 컴포넌트를 만든다. (자동 파괴 안 함, 엔진은 사운드 없이 만들 수 없어 처음 재생할 사운드를 받는다) */
	UAudioComponent* CreateAudioComponent(USoundBase* Sound) const;

	UPROPERTY()
	TObjectPtr<UODAudioSet> AudioSet;

	/** 크로스페이드용 배경음 컴포넌트 두 개. ActiveMusicIndex가 지금 재생 중인 쪽 */
	UPROPERTY()
	TObjectPtr<UAudioComponent> MusicComponents[2];

	UPROPERTY()
	TObjectPtr<UAudioComponent> AmbienceComponent;

	int32 ActiveMusicIndex = 0;

	/** 지금 재생(또는 페이드인) 중인 곡. 같은 곡 재요청을 거르는 데 쓴다. */
	TWeakObjectPtr<USoundBase> CurrentTrack;

	TSharedPtr<FStreamableHandle> LoadHandle;
};
