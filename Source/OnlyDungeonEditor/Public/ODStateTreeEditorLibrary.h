// OnlyDungeon 에디터 도구: StateTree 구성

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ODStateTreeEditorLibrary.generated.h"

class UStateTree;
class UStateTreeState;

/**
 * 에디터 Python(MCP)에서 호출하는 StateTree 편집 도구.
 * UE 5.9 스크립팅에는 상태·태스크·조건 추가 API는 있지만 전이 추가와 선택 방식 지정이 없어 C++로 제공한다.
 * Python: unreal.ODStateTreeEditorLibrary.add_transition(...)
 * 수정 뒤에는 StateTree 에셋을 수정 표시(mark_as_modified)하고 컴파일·저장한다.
 */
UCLASS()
class ONLYDUNGEONEDITOR_API UODStateTreeEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 상태에 전이를 추가한다.
	 * @param Trigger     EStateTreeTransitionTrigger 이름 (OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick)
	 * @param Type        EStateTreeTransitionType 이름 (GotoState, Succeeded, Failed, NextState, NextSelectableState, Parent)
	 * @param TargetState GotoState일 때 이동할 상태
	 * @return 성공 여부 (이름이 잘못되면 false)
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Editor|StateTree")
	static bool AddTransition(UStateTreeState* State, const FString& Trigger, const FString& Type, UStateTreeState* TargetState);

	/** 상태의 전이를 모두 지운다. */
	UFUNCTION(BlueprintCallable, Category = "OD|Editor|StateTree")
	static void ClearTransitions(UStateTreeState* State);

	/**
	 * 상태의 하위 상태 선택 방식을 바꾼다.
	 * @param Behavior EStateTreeStateSelectionBehavior 이름 (TrySelectChildrenInOrder, TrySelectChildrenAtRandom, TryEnterState 등)
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Editor|StateTree")
	static bool SetSelectionBehavior(UStateTreeState* State, const FString& Behavior);

	/** 상태 계층·태스크·조건·전이를 글로 정리한다. (검증용) */
	UFUNCTION(BlueprintCallable, Category = "OD|Editor|StateTree")
	static FString DescribeStateTree(UStateTree* StateTree);
};
