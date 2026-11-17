// OnlyDungeon 적 AI용 StateTree 노드 (조건·태스크)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"
#include "ODStateTreeNodes.generated.h"

class AActor;

/*
 * 공통 규칙
 * - 모든 노드는 Context의 Actor(= 적 캐릭터, AI 스키마의 기본 컨텍스트)만 쓴다. 다른 노드 출력과 바인딩하지 않아도 동작하도록,
 *   대상(플레이어)은 적 캐릭터가 직접 관리한다. (AODEnemyCharacter::UpdateCombatTarget)
 * - 공격·회피는 플레이어와 같은 어빌리티를 InputTag로 발동한다. (같은 무기 어빌리티 세트 사용)
 */

// ---------------------------------------------------------------- 조건

USTRUCT()
struct FODStateTreeHasTargetConditionInstanceData
{
	GENERATED_BODY()

	/** 적 캐릭터 */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 참이면 대상이 없을 때 통과 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/** 공격 대상(감지 범위 안의 플레이어)이 있는지 */
USTRUCT(meta = (DisplayName = "OD Has Combat Target", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeHasTargetCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FODStateTreeHasTargetConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FODStateTreeTargetInRangeConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 대상까지의 수평 거리가 이 값 이하면 통과 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxRange = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/** 대상이 사거리 안에 있는지 */
USTRUCT(meta = (DisplayName = "OD Target In Range", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeTargetInRangeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FODStateTreeTargetInRangeConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FODStateTreeHasTagConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 적 ASC가 이 태그를 가지고 있으면 통과 (State.Phase.2, State.Stunned 등) */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/** 적이 게임플레이 태그를 가지고 있는지 (페이즈·경직 판별) */
USTRUCT(meta = (DisplayName = "OD Has Gameplay Tag", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeHasTagCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FODStateTreeHasTagConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

// ---------------------------------------------------------------- 태스크

USTRUCT()
struct FODStateTreeChaseTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 대상과 이 거리 안으로 들어오면 성공 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "10.0", Units = "cm"))
	float AcceptanceRadius = 150.0f;
};

/** 대상을 향해 길찾기로 이동한다. 사거리 안에 들어오면 성공, 대상을 잃으면 실패 */
USTRUCT(meta = (DisplayName = "OD Chase Target", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeChaseTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FODStateTreeChaseTargetTask() { bShouldCallTick = true; }

	using FInstanceDataType = FODStateTreeChaseTargetTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FODStateTreeReturnHomeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 시작 위치와 이 거리 안이면 도착 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "10.0", Units = "cm"))
	float AcceptanceRadius = 100.0f;

	/** 도착한 뒤 이 시간 동안 대기하고 성공한다. (대기 상태가 매 틱 다시 선택되지 않게) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "s"))
	float IdleTime = 0.4f;

	float IdleElapsed = 0.0f;
};

/** 대상이 없을 때: 시작 위치로 돌아가 잠시 대기. 그 사이 대상을 찾으면 바로 성공해 다시 선택하게 한다. */
USTRUCT(meta = (DisplayName = "OD Return Home", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeReturnHomeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FODStateTreeReturnHomeTask() { bShouldCallTick = true; }

	using FInstanceDataType = FODStateTreeReturnHomeTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FODStateTreeActivateAbilityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 발동할 어빌리티의 입력 태그 (InputTag.Attack.Light 등. 플레이어와 같은 어빌리티를 쓴다) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	/** 입력 횟수. 콤보는 2 이상이면 그만큼 이어 친다. (첫 발동 포함) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "1"))
	int32 PressCount = 1;

	/** 콤보 이어 치기 입력 간격 (콤보 어빌리티가 입력을 예약해 두므로 대략이면 된다) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.05", Units = "s"))
	float PressInterval = 0.3f;

	/** 이 시간이 지나도 어빌리티가 안 끝나면 실패 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.5", Units = "s"))
	float Timeout = 8.0f;

	int32 Presses = 0;
	float Elapsed = 0.0f;
	float SinceLastPress = 0.0f;
};

/** 대상을 바라보고 어빌리티를 발동한다. 어빌리티가 끝나면 성공, 발동하지 못하면 실패 */
USTRUCT(meta = (DisplayName = "OD Activate Ability", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeActivateAbilityTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FODStateTreeActivateAbilityTask() { bShouldCallTick = true; }

	using FInstanceDataType = FODStateTreeActivateAbilityTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

// ---------------------------------------------------------------- 패턴 전투 (UODEnemyCombatComponent 사용)

USTRUCT()
struct FODStateTreeCanAttackConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 패턴 사거리보다 이만큼 멀어도 "걸어가서 쓸" 수 있는 것으로 본다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
	float ApproachSlack = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/** 공격을 시작할 수 있는지: 대상이 있고, 공통 공격 간격이 지났고(순번도 얻을 수 있고), 지금 거리에서 쓸 패턴이 있다. */
USTRUCT(meta = (DisplayName = "OD Can Attack", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeCanAttackCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FODStateTreeCanAttackConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FODStateTreeTargetOpeningConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 빈틈 공략 패턴 사거리보다 이만큼 멀어도 공략한다. (돌진으로 거리를 좁힘) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
	float ApproachSlack = 300.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bInvert = false;
};

/** 대상에게 빈틈(물약 마시기·후딜·경직)이 생겼고 이번 빈틈을 공략하기로 했는지 */
USTRUCT(meta = (DisplayName = "OD Target Opening", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeTargetOpeningCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FODStateTreeTargetOpeningConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FODStateTreeExecutePatternTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 빈틈 공략 패턴만 고른다. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bPunish = false;

	/** 패턴 사거리보다 이만큼 멀면 걸어서 다가간 뒤 쓴다. 더 멀면 고르지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "cm"))
	float ApproachSlack = 600.0f;

	/** 다가가는 데 이보다 오래 걸리면 포기한다. */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.1", Units = "s"))
	float ApproachTimeout = 3.0f;

	/** 어빌리티가 이 시간 안에 안 끝나면 실패 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.5", Units = "s"))
	float Timeout = 12.0f;

	int32 PatternIndex = INDEX_NONE;
	bool bStarted = false;
	int32 Presses = 0;
	float Elapsed = 0.0f;
	float SinceLastPress = 0.0f;
};

/**
 * 적 전투 컴포넌트에서 패턴을 하나 골라 실행한다. 사거리 밖이면 먼저 다가간다.
 * 어빌리티가 끝나면 성공(경직으로 끊겨도 성공), 쓸 패턴이 없거나 다가가지 못하면 실패. 어떻게 끝나든 공통 공격 간격이 시작된다.
 */
USTRUCT(meta = (DisplayName = "OD Execute Pattern", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeExecutePatternTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FODStateTreeExecutePatternTask() { bShouldCallTick = true; }

	using FInstanceDataType = FODStateTreeExecutePatternTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

USTRUCT()
struct FODStateTreeStrafeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 최소 이 시간은 돈 뒤에야 공격·빈틈 공략으로 넘어간다. (들어오자마자 끊기면 어색하다) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", Units = "s"))
	float MinDuration = 0.6f;

	float Duration = 0.0f;
	float Elapsed = 0.0f;
	float Direction = 1.0f;
	float StuckTime = 0.0f;
	float SavedMaxWalkSpeed = -1.0f;
	bool bSavedOrientToMovement = true;
	bool bSavedUseControllerDesiredRotation = false;
};

/**
 * 간 보기: 대상을 바라본 채 일정 거리를 유지하며 좌우로 돈다. (거리·시간·속도는 패턴 데이터)
 * 시간이 다 되거나, 최소 시간이 지난 뒤 공격할 수 있게 되거나 빈틈이 보이면 성공으로 끝나 상위 상태가 다시 고르게 한다.
 * 막혀서 못 움직이면 방향을 바꾼다.
 */
USTRUCT(meta = (DisplayName = "OD Strafe", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeStrafeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FODStateTreeStrafeTask() { bShouldCallTick = true; }

	using FInstanceDataType = FODStateTreeStrafeTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

// ---------------------------------------------------------------- 발견 전 행동

USTRUCT()
struct FODStateTreeIdleBehaviorTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AActor> Actor;

	/** 지점 도착 판정 거리 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "10.0", Units = "cm"))
	float AcceptanceRadius = 100.0f;

	int32 PatrolIndex = 0;
	float WaitElapsed = 0.0f;
	bool bLying = false;
	bool bGettingUp = false;
};

/**
 * 대상이 없을 때의 행동 (적의 IdleBehavior): 서 있기 / 순찰 / 매복(쓰러져 있다가 일어나기).
 * 구역을 벗어나 돌아오는 중이면 먼저 시작 위치로 간다. 대상이 생기면 성공(매복은 일어나는 동작이 끝난 뒤)으로 끝나 상위 상태가 추적·공격을 고르게 한다.
 */
USTRUCT(meta = (DisplayName = "OD Idle Behavior", Category = "OnlyDungeon"))
struct ONLYDUNGEON_API FODStateTreeIdleBehaviorTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FODStateTreeIdleBehaviorTask() { bShouldCallTick = true; }

	using FInstanceDataType = FODStateTreeIdleBehaviorTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
