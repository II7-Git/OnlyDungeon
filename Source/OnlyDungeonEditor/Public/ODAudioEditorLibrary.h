// OnlyDungeon 에디터 도구: 사운드 큐 구성

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ODAudioEditorLibrary.generated.h"

class USoundCue;
class USoundWave;

/**
 * 에디터 Python(MCP)에서 호출하는 사운드 큐 편집 도구.
 * UE 5.9 Python의 SoundCue 팩토리는 사운드 하나짜리 큐만 만들 수 있어, 여러 소리를 번갈아 내는 큐를 C++로 구성한다.
 * Python: unreal.ODAudioEditorLibrary.build_random_sound_cue(...)
 */
UCLASS()
class ONLYDUNGEONEDITOR_API UODAudioEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 큐의 노드를 모두 지우고 "Modulator(음높이·크기 흔들기) → Random(겹치지 않게 무작위) → 사운드들" 구조로 다시 만든다.
	 * 같은 소리가 연달아 반복되는 기계적인 느낌을 줄이기 위해 매번 다른 소리를 약간 다른 음높이로 낸다.
	 * @param Cue       대상 사운드 큐 (감쇠·동시 재생·사운드 클래스 설정은 그대로 둔다)
	 * @param Waves     번갈아 낼 사운드 (1개 이상)
	 * @param PitchMin  음높이 배율 하한 (1 = 원래 음높이)
	 * @param PitchMax  음높이 배율 상한
	 * @param VolumeMin 크기 배율 하한
	 * @param VolumeMax 크기 배율 상한
	 * @param bLooping  한 소리를 반복 재생할지 (환경음·배경음용, 이 경우 Random은 쓰지 않고 첫 사운드만 반복한다)
	 * @return 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Editor|Audio")
	static bool BuildRandomSoundCue(USoundCue* Cue, const TArray<USoundWave*>& Waves, float PitchMin = 0.95f, float PitchMax = 1.05f, float VolumeMin = 0.9f, float VolumeMax = 1.0f, bool bLooping = false);
};
