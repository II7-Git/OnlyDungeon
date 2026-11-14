// OnlyDungeon 에디터 도구: 레벨 시퀀스 설정

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ODSequenceEditorLibrary.generated.h"

class ULevelSequence;

/**
 * 에디터 Python(MCP)에서 호출하는 레벨 시퀀스 편집 도구.
 * Python에 노출되지 않는 설정을 C++로 제공한다.
 * Python: unreal.ODSequenceEditorLibrary.set_camera_cut_can_blend(seq, True)
 */
UCLASS()
class ONLYDUNGEONEDITOR_API UODSequenceEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 카메라 컷 트랙의 "블렌드 허용(Can Blend)"을 설정한다.
	 * 켜면 카메라 컷 섹션의 이즈 인/아웃 동안 게임 카메라와 시퀀스 카메라가 블렌드된다.
	 * @return 카메라 컷 트랙을 찾아 설정했으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Editor|Sequence")
	static bool SetCameraCutCanBlend(ULevelSequence* Sequence, bool bCanBlend);

	/** 카메라 컷 트랙의 "블렌드 허용" 값 (트랙이 없으면 false) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Editor|Sequence")
	static bool GetCameraCutCanBlend(ULevelSequence* Sequence);
};
