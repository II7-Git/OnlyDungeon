// OnlyDungeon 에디터 도구: 애님 몽타주 구성

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ODMontageEditorLibrary.generated.h"

class UAnimMontage;
class UAnimSequenceBase;

/**
 * 에디터 Python(MCP)에서 호출하는 몽타주 편집 도구.
 * UE 5.9 Python에는 몽타주 섹션을 만드는 API가 없어 C++로 제공한다.
 * Python: unreal.ODMontageEditorLibrary.build_sectioned_montage(...)
 */
UCLASS()
class ONLYDUNGEONEDITOR_API UODMontageEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 몽타주의 첫 슬롯 트랙을 주어진 시퀀스들로 다시 채우고, 시퀀스마다 섹션을 하나씩 만든다.
	 * 기존 세그먼트와 섹션은 모두 지워진다. (노티파이는 유지)
	 * @param Montage           대상 몽타주 (슬롯이 없으면 DefaultSlot을 만든다)
	 * @param Sequences         순서대로 이어 붙일 애니메이션
	 * @param SectionNames      각 시퀀스 시작 위치에 만들 섹션 이름 (Sequences와 같은 개수)
	 * @param NextSectionNames  각 섹션이 끝난 뒤 이어질 섹션 (None이면 거기서 몽타주 종료)
	 * @return 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Editor|Montage")
	static bool BuildSectionedMontage(UAnimMontage* Montage, const TArray<UAnimSequenceBase*>& Sequences, const TArray<FName>& SectionNames, const TArray<FName>& NextSectionNames);

	/**
	 * 시퀀스의 일부 구간(StartTime~EndTime)만 재생하는 몽타주로 구성한다. 슬롯 이름도 바꾼다.
	 * 기존 세그먼트·섹션은 지워지고 "Default" 섹션 하나가 된다. (노티파이는 유지)
	 * 긴 원본 모션에서 필요한 부분만 쓸 때 사용한다. (예: 8초 마시기 모션의 들이켜는 구간)
	 * @param SlotName  몽타주가 재생될 슬롯 (예: UpperBody = 상체만)
	 * @param PlayRate  구간 재생 속도 (몽타주 길이가 이만큼 줄어든다)
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Editor|Montage")
	static bool BuildRangeMontage(UAnimMontage* Montage, UAnimSequenceBase* Sequence, float StartTime, float EndTime, FName SlotName, float PlayRate = 1.0f);

	/** 섹션 시작 시간(초) 조회. 없는 섹션이면 -1 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Editor|Montage")
	static float GetSectionStartTime(const UAnimMontage* Montage, FName SectionName);
};
