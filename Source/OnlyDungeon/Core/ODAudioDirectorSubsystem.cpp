// OnlyDungeon 배경음·환경음 진행 서브시스템

#include "ODAudioDirectorSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Core/ODAudioSet.h"
#include "Core/ODBossEncounterSubsystem.h"
#include "Core/ODGameMode.h"
#include "Core/ODLogChannels.h"

namespace ODAudioDirector
{
	/** 레벨 시작·부활 때 배경음이 들어오는 시간 */
	constexpr float StartFadeTime = 1.0f;

	/** 보스전이 끝난 뒤 환경음을 되돌리는 시간 */
	constexpr float AmbienceRestoreTime = 2.0f;

	/** 페이드 시간 하한 (0이면 오디오 컴포넌트가 페이드를 건너뛰지 않고 한 프레임에 끊기므로 아주 짧게라도 준다) */
	constexpr float MinFadeTime = 0.05f;
}

bool UODAudioDirectorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UODAudioDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UODBossEncounterSubsystem* Encounter = InWorld.GetSubsystem<UODBossEncounterSubsystem>())
	{
		Encounter->OnEncounterStarted.AddUniqueDynamic(this, &ThisClass::HandleBossEncounterStarted);
		Encounter->OnEncounterEnded.AddUniqueDynamic(this, &ThisClass::HandleBossEncounterEnded);
	}

	const AODGameMode* GameMode = InWorld.GetAuthGameMode<AODGameMode>();
	const TSoftObjectPtr<UODAudioSet> SetRef = IsValid(GameMode) ? GameMode->GetAudioSet() : TSoftObjectPtr<UODAudioSet>();
	if (SetRef.IsNull())
	{
		UE_LOG(LogOD, Verbose, TEXT("오디오 세트가 지정되지 않아 배경음을 재생하지 않습니다."));
		return;
	}

	// 세트를 먼저 읽고, 세트 안의 사운드들을 이어서 읽는다. (둘 다 비동기)
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(SetRef.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, SetRef]()
	{
		AudioSet = SetRef.Get();
		if (!IsValid(AudioSet))
		{
			UE_LOG(LogOD, Warning, TEXT("오디오 세트를 불러오지 못했습니다: %s"), *SetRef.ToString());
			return;
		}

		TArray<FSoftObjectPath> SoundPaths;
		SoundPaths.Reserve(5);
		for (const TSoftObjectPtr<USoundBase>* Sound : { &AudioSet->FieldMusic, &AudioSet->BossMusic, &AudioSet->VictoryStinger, &AudioSet->DeathStinger, &AudioSet->FieldAmbience })
		{
			if (!Sound->IsNull())
			{
				SoundPaths.Add(Sound->ToSoftObjectPath());
			}
		}
		LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(SoundPaths), FStreamableDelegate::CreateUObject(this, &ThisClass::HandleAudioSetLoaded));
	}));
}

void UODAudioDirectorSubsystem::Deinitialize()
{
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		if (UODBossEncounterSubsystem* Encounter = World->GetSubsystem<UODBossEncounterSubsystem>())
		{
			Encounter->OnEncounterStarted.RemoveDynamic(this, &ThisClass::HandleBossEncounterStarted);
			Encounter->OnEncounterEnded.RemoveDynamic(this, &ThisClass::HandleBossEncounterEnded);
		}
	}

	for (TObjectPtr<UAudioComponent>& Music : MusicComponents)
	{
		if (IsValid(Music))
		{
			Music->Stop();
		}
	}
	if (IsValid(AmbienceComponent))
	{
		AmbienceComponent->Stop();
	}
	CurrentTrack.Reset();
	Super::Deinitialize();
}

void UODAudioDirectorSubsystem::HandleAudioSetLoaded()
{
	if (!IsValid(AudioSet))
	{
		return;
	}

	// 보스방 테스트처럼 이미 보스전이 진행 중이면 보스 배경음부터 시작한다.
	const UWorld* World = GetWorld();
	const UODBossEncounterSubsystem* Encounter = World ? World->GetSubsystem<UODBossEncounterSubsystem>() : nullptr;
	const bool bBossActive = Encounter && Encounter->IsEncounterActive();

	CrossfadeTo(bBossActive ? AudioSet->BossMusic.Get() : AudioSet->FieldMusic.Get(), ODAudioDirector::StartFadeTime);
	SetAmbienceVolume(bBossActive ? AudioSet->BossAmbienceVolume : 1.0f, ODAudioDirector::StartFadeTime);
	UE_LOG(LogOD, Log, TEXT("오디오 세트 로드 완료: %s"), *GetNameSafe(AudioSet));
}

void UODAudioDirectorSubsystem::NotifyPlayerDied()
{
	if (!IsValid(AudioSet))
	{
		return;
	}
	CrossfadeTo(nullptr, AudioSet->StopFadeTime);
	PlayStinger(AudioSet->DeathStinger.Get());
}

void UODAudioDirectorSubsystem::NotifyPlayerRevived()
{
	if (!IsValid(AudioSet))
	{
		return;
	}
	CrossfadeTo(AudioSet->FieldMusic.Get(), ODAudioDirector::StartFadeTime);
	SetAmbienceVolume(1.0f, ODAudioDirector::StartFadeTime);
}

void UODAudioDirectorSubsystem::HandleBossEncounterStarted(AODEnemyCharacter* Boss)
{
	if (!IsValid(AudioSet))
	{
		return;
	}
	CrossfadeTo(AudioSet->BossMusic.Get(), AudioSet->CrossfadeTime);
	SetAmbienceVolume(AudioSet->BossAmbienceVolume, AudioSet->CrossfadeTime);
}

void UODAudioDirectorSubsystem::HandleBossEncounterEnded(bool bVictory)
{
	if (!IsValid(AudioSet))
	{
		return;
	}

	if (bVictory)
	{
		// 승리: 효과음 한 번 뒤 정적. 환경음만 돌아온다.
		CrossfadeTo(nullptr, AudioSet->StopFadeTime);
		PlayStinger(AudioSet->VictoryStinger.Get());
	}
	else
	{
		// 패배(부활·테스트 초기화): 필드로 돌아간다. 부활 알림이 뒤따라와도 같은 곡이라 겹치지 않는다.
		CrossfadeTo(AudioSet->FieldMusic.Get(), AudioSet->CrossfadeTime);
	}
	SetAmbienceVolume(1.0f, ODAudioDirector::AmbienceRestoreTime);
}

void UODAudioDirectorSubsystem::CrossfadeTo(USoundBase* Track, float FadeTime)
{
	if (CurrentTrack.Get() == Track)
	{
		return;
	}
	const float SafeFadeTime = FMath::Max(FadeTime, ODAudioDirector::MinFadeTime);

	// 지금 곡은 줄여서 멈춘다.
	if (UAudioComponent* Active = MusicComponents[ActiveMusicIndex]; IsValid(Active) && Active->IsPlaying())
	{
		Active->FadeOut(SafeFadeTime, 0.0f);
	}
	CurrentTrack = Track;
	if (!IsValid(Track))
	{
		return;
	}

	// 다른 컴포넌트로 새 곡을 키운다.
	ActiveMusicIndex = 1 - ActiveMusicIndex;
	TObjectPtr<UAudioComponent>& Next = MusicComponents[ActiveMusicIndex];
	if (!IsValid(Next))
	{
		Next = CreateAudioComponent(Track);
	}
	if (IsValid(Next))
	{
		Next->Stop();
		Next->SetSound(Track);
		Next->FadeIn(SafeFadeTime, 1.0f);
		UE_LOG(LogOD, Log, TEXT("배경음 전환: %s"), *GetNameSafe(Track));
	}
}

void UODAudioDirectorSubsystem::SetAmbienceVolume(float Volume, float FadeTime)
{
	if (!IsValid(AudioSet) || AudioSet->FieldAmbience.IsNull())
	{
		return;
	}
	const float SafeFadeTime = FMath::Max(FadeTime, ODAudioDirector::MinFadeTime);

	if (!IsValid(AmbienceComponent))
	{
		AmbienceComponent = CreateAudioComponent(AudioSet->FieldAmbience.Get());
		if (!IsValid(AmbienceComponent))
		{
			return;
		}
	}

	if (!AmbienceComponent->IsPlaying())
	{
		if (Volume > 0.0f)
		{
			AmbienceComponent->FadeIn(SafeFadeTime, Volume);
		}
		return;
	}
	AmbienceComponent->AdjustVolume(SafeFadeTime, Volume);
}

void UODAudioDirectorSubsystem::PlayStinger(USoundBase* Sound) const
{
	if (IsValid(Sound))
	{
		UGameplayStatics::PlaySound2D(this, Sound, 1.0f, 1.0f, 0.0f, nullptr, nullptr, false);
	}
}

UAudioComponent* UODAudioDirectorSubsystem::CreateAudioComponent(USoundBase* Sound) const
{
	// 레벨이 바뀌면 함께 정리되도록 유지하지 않고, 곡을 바꿔 다시 쓰므로 자동 파괴하지 않는다.
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(this, Sound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
	if (IsValid(Component))
	{
		Component->bIsUISound = false;
	}
	return Component;
}
