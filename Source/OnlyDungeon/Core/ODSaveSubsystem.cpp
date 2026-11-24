// OnlyDungeon 저장 서브시스템

#include "ODSaveSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Core/ODAudioDirectorSubsystem.h"
#include "Core/ODLogChannels.h"
#include "Core/ODSaveGame.h"

const FString UODSaveSubsystem::ProgressSlot(TEXT("OD_Progress"));
const FString UODSaveSubsystem::SettingsSlot(TEXT("OD_Settings"));

namespace ODSave
{
	constexpr int32 UserIndex = 0;
}

void UODSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Settings = Cast<UODSettingsSave>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, ODSave::UserIndex));
	if (!IsValid(Settings))
	{
		Settings = Cast<UODSettingsSave>(UGameplayStatics::CreateSaveGameObject(UODSettingsSave::StaticClass()));
	}
}

bool UODSaveSubsystem::HasProgress() const
{
	if (!UGameplayStatics::DoesSaveGameExist(ProgressSlot, ODSave::UserIndex))
	{
		return false;
	}
	// 버전 확인을 위해 읽는다. (타이틀에서 한 번 불리는 정도라 비용은 문제없다)
	const UODSaveGame* Loaded = Cast<UODSaveGame>(UGameplayStatics::LoadGameFromSlot(ProgressSlot, ODSave::UserIndex));
	return IsValid(Loaded) && Loaded->SaveVersion == UODSaveGame::CurrentVersion;
}

UODSaveGame* UODSaveSubsystem::LoadProgress()
{
	UODSaveGame* Loaded = Cast<UODSaveGame>(UGameplayStatics::LoadGameFromSlot(ProgressSlot, ODSave::UserIndex));
	if (!IsValid(Loaded) || Loaded->SaveVersion != UODSaveGame::CurrentVersion)
	{
		UE_LOG(LogOD, Warning, TEXT("진행 저장을 불러오지 못했습니다. (없음·손상·버전 다름)"));
		return nullptr;
	}
	Progress = Loaded;
	return Progress;
}

UODSaveGame* UODSaveSubsystem::CreateNewProgress()
{
	if (UGameplayStatics::DoesSaveGameExist(ProgressSlot, ODSave::UserIndex))
	{
		UGameplayStatics::DeleteGameInSlot(ProgressSlot, ODSave::UserIndex);
	}
	Progress = Cast<UODSaveGame>(UGameplayStatics::CreateSaveGameObject(UODSaveGame::StaticClass()));
	return Progress;
}

void UODSaveSubsystem::SaveProgress(bool bSync)
{
	if (!IsValid(Progress))
	{
		return;
	}
	if (bSync)
	{
		UGameplayStatics::SaveGameToSlot(Progress, ProgressSlot, ODSave::UserIndex);
		return;
	}
	if (bSaving)
	{
		bSaveQueued = true;
		return;
	}

	bSaving = true;
	FAsyncSaveGameToSlotDelegate OnSaved = FAsyncSaveGameToSlotDelegate::CreateWeakLambda(this, [this](const FString&, const int32, bool bSuccess)
	{
		bSaving = false;
		if (!bSuccess)
		{
			UE_LOG(LogOD, Warning, TEXT("진행 저장 실패"));
		}
		if (bSaveQueued)
		{
			bSaveQueued = false;
			SaveProgress(false);
		}
	});
	UGameplayStatics::AsyncSaveGameToSlot(Progress, ProgressSlot, ODSave::UserIndex, OnSaved);
}

float UODSaveSubsystem::GetVolume(EODVolumeChannel Channel) const
{
	if (!IsValid(Settings))
	{
		return 1.0f;
	}
	switch (Channel)
	{
	case EODVolumeChannel::Music:    return Settings->MusicVolume;
	case EODVolumeChannel::SFX:      return Settings->SFXVolume;
	case EODVolumeChannel::Ambience: return Settings->AmbienceVolume;
	}
	return 1.0f;
}

void UODSaveSubsystem::SetVolume(EODVolumeChannel Channel, float Volume)
{
	if (!IsValid(Settings))
	{
		return;
	}
	const float Clamped = FMath::Clamp(Volume, 0.0f, 1.0f);
	switch (Channel)
	{
	case EODVolumeChannel::Music:    Settings->MusicVolume = Clamped; break;
	case EODVolumeChannel::SFX:      Settings->SFXVolume = Clamped; break;
	case EODVolumeChannel::Ambience: Settings->AmbienceVolume = Clamped; break;
	}
	ApplyVolumes();
}

void UODSaveSubsystem::SaveSettings()
{
	if (IsValid(Settings))
	{
		UGameplayStatics::AsyncSaveGameToSlot(Settings, SettingsSlot, ODSave::UserIndex);
	}
}

void UODSaveSubsystem::ApplyVolumes() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (UODAudioDirectorSubsystem* Audio = World ? World->GetSubsystem<UODAudioDirectorSubsystem>() : nullptr)
	{
		Audio->ApplyVolumes(GetVolume(EODVolumeChannel::Music), GetVolume(EODVolumeChannel::SFX), GetVolume(EODVolumeChannel::Ambience));
	}
}
