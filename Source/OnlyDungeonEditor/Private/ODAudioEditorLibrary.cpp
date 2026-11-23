// OnlyDungeon 에디터 도구: 사운드 큐 구성

#include "ODAudioEditorLibrary.h"

#include "Sound/SoundCue.h"
#include "Sound/SoundNodeLooping.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"

bool UODAudioEditorLibrary::BuildRandomSoundCue(USoundCue* Cue, const TArray<USoundWave*>& Waves, float PitchMin, float PitchMax, float VolumeMin, float VolumeMax, bool bLooping)
{
	TArray<USoundWave*, TInlineAllocator<32>> ValidWaves;
	for (USoundWave* Wave : Waves)
	{
		if (IsValid(Wave))
		{
			ValidWaves.Add(Wave);
		}
	}
	if (!IsValid(Cue) || ValidWaves.IsEmpty())
	{
		return false;
	}

	Cue->Modify();
	Cue->FirstNode = nullptr;
	Cue->AllNodes.Reset();

	USoundNodeModulator* Modulator = Cue->ConstructSoundNode<USoundNodeModulator>();
	Modulator->PitchMin = FMath::Min(PitchMin, PitchMax);
	Modulator->PitchMax = FMath::Max(PitchMin, PitchMax);
	Modulator->VolumeMin = FMath::Min(VolumeMin, VolumeMax);
	Modulator->VolumeMax = FMath::Max(VolumeMin, VolumeMax);
	Cue->FirstNode = Modulator;

	auto MakePlayer = [Cue](USoundWave* Wave)
	{
		USoundNodeWavePlayer* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
		Player->SetSoundWave(Wave);
		return Player;
	};

	Modulator->InsertChildNode(0);
	if (bLooping)
	{
		// 반복: 웨이브 플레이어 자체 반복을 켜 끊김 없이 이어지게 한다.
		USoundNodeWavePlayer* Player = MakePlayer(ValidWaves[0]);
		Player->bLooping = true;
		Modulator->ChildNodes[0] = Player;
	}
	else if (ValidWaves.Num() == 1)
	{
		Modulator->ChildNodes[0] = MakePlayer(ValidWaves[0]);
	}
	else
	{
		USoundNodeRandom* Random = Cue->ConstructSoundNode<USoundNodeRandom>();
		Random->bRandomizeWithoutReplacement = true;
		Modulator->ChildNodes[0] = Random;
		const int32 Count = FMath::Min(ValidWaves.Num(), Random->GetMaxChildNodes());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Random->InsertChildNode(Index);
			Random->ChildNodes[Index] = MakePlayer(ValidWaves[Index]);
		}
	}

	// 사운드 노드 구조로 에디터 그래프를 다시 만들고, 길이 등 캐시 값을 갱신한다.
	Cue->LinkGraphNodesFromSoundNodes();
	Cue->CacheAggregateValues();
	Cue->PostEditChange();
	Cue->MarkPackageDirty();
	return true;
}
