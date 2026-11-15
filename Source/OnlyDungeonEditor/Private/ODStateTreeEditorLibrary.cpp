// OnlyDungeon 에디터 도구: StateTree 구성

#include "ODStateTreeEditorLibrary.h"

#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeTypes.h"

namespace ODStateTreeEditor
{
	template <typename TEnum>
	bool ParseEnum(const FString& Name, TEnum& OutValue)
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		const int64 Value = Enum ? Enum->GetValueByNameString(Name) : INDEX_NONE;
		if (Value == INDEX_NONE)
		{
			return false;
		}
		OutValue = static_cast<TEnum>(Value);
		return true;
	}

	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Value)) : FString();
	}

	void DescribeState(const UStateTreeState* State, int32 Depth, FString& Out)
	{
		if (!State)
		{
			return;
		}
		const FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
		Out += FString::Printf(TEXT("%s[%s] select=%s\n"), *Indent, *State->Name.ToString(), *EnumName(State->SelectionBehavior));
		for (const FStateTreeEditorNode& Node : State->EnterConditions)
		{
			Out += FString::Printf(TEXT("%s  cond: %s\n"), *Indent, Node.Node.GetScriptStruct() ? *Node.Node.GetScriptStruct()->GetName() : TEXT("?"));
		}
		for (const FStateTreeEditorNode& Node : State->Tasks)
		{
			Out += FString::Printf(TEXT("%s  task: %s\n"), *Indent, Node.Node.GetScriptStruct() ? *Node.Node.GetScriptStruct()->GetName() : TEXT("?"));
		}
		for (const FStateTreeTransition& Transition : State->Transitions)
		{
			Out += FString::Printf(TEXT("%s  -> %s %s %s\n"), *Indent, *EnumName(Transition.Trigger), *EnumName(Transition.State.LinkType), *Transition.State.Name.ToString());
		}
		for (const UStateTreeState* Child : State->Children)
		{
			DescribeState(Child, Depth + 1, Out);
		}
	}
}

bool UODStateTreeEditorLibrary::AddTransition(UStateTreeState* State, const FString& Trigger, const FString& Type, UStateTreeState* TargetState)
{
	if (!State)
	{
		return false;
	}

	EStateTreeTransitionTrigger TriggerValue;
	EStateTreeTransitionType TypeValue;
	if (!ODStateTreeEditor::ParseEnum(Trigger, TriggerValue) || !ODStateTreeEditor::ParseEnum(Type, TypeValue))
	{
		return false;
	}
	if (TypeValue == EStateTreeTransitionType::GotoState && !TargetState)
	{
		return false;
	}

	State->Modify();
	State->AddTransition(TriggerValue, TypeValue, TargetState);
	return true;
}

void UODStateTreeEditorLibrary::ClearTransitions(UStateTreeState* State)
{
	if (State)
	{
		State->Modify();
		State->Transitions.Reset();
	}
}

bool UODStateTreeEditorLibrary::SetSelectionBehavior(UStateTreeState* State, const FString& Behavior)
{
	EStateTreeStateSelectionBehavior Value;
	if (!State || !ODStateTreeEditor::ParseEnum(Behavior, Value))
	{
		return false;
	}
	State->Modify();
	State->SelectionBehavior = Value;
	return true;
}

FString UODStateTreeEditorLibrary::DescribeStateTree(UStateTree* StateTree)
{
	const UStateTreeEditorData* EditorData = StateTree ? Cast<UStateTreeEditorData>(StateTree->EditorData) : nullptr;
	if (!EditorData)
	{
		return TEXT("(편집 데이터 없음)");
	}

	FString Out;
	for (const UStateTreeState* SubTree : EditorData->SubTrees)
	{
		ODStateTreeEditor::DescribeState(SubTree, 0, Out);
	}
	return Out;
}
