// OnlyDungeon 적 AI용 StateTree 노드

#include "ODStateTreeNodes.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Character/ODEnemyCharacter.h"
#include "Core/ODLogChannels.h"

namespace ODStateTreeNodes
{
	AODEnemyCharacter* GetEnemy(AActor* Actor)
	{
		AODEnemyCharacter* Enemy = Cast<AODEnemyCharacter>(Actor);
		return IsValid(Enemy) && !Enemy->IsDead() ? Enemy : nullptr;
	}

	AAIController* GetController(const AODEnemyCharacter* Enemy)
	{
		return IsValid(Enemy) ? Cast<AAIController>(Enemy->GetController()) : nullptr;
	}

	float DistanceToTarget2D(const AActor* From, const AActor* To)
	{
		return FVector::Dist2D(From->GetActorLocation(), To->GetActorLocation());
	}

	/** 대상 쪽으로 즉시 돌아선다. (공격 직전) */
	void FaceTarget(AODEnemyCharacter* Enemy, const AActor* Target)
	{
		FVector ToTarget = Target->GetActorLocation() - Enemy->GetActorLocation();
		ToTarget.Z = 0.0;
		if (!ToTarget.IsNearlyZero())
		{
			Enemy->SetActorRotation(ToTarget.Rotation());
		}
	}
}

// ---------------------------------------------------------------- 조건

bool FODStateTreeHasTargetCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	const bool bHasTarget = Enemy && IsValid(Enemy->UpdateCombatTarget());
	return Data.bInvert ? !bHasTarget : bHasTarget;
}

bool FODStateTreeTargetInRangeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	const AActor* Target = Enemy ? Enemy->UpdateCombatTarget() : nullptr;
	const bool bInRange = IsValid(Target) && ODStateTreeNodes::DistanceToTarget2D(Enemy, Target) <= Data.MaxRange;
	return Data.bInvert ? !bInRange : bInRange;
}

bool FODStateTreeHasTagCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	const AODEnemyCharacter* Enemy = Cast<AODEnemyCharacter>(Data.Actor);
	const UODAbilitySystemComponent* ASC = IsValid(Enemy) ? Enemy->GetODAbilitySystemComponent() : nullptr;
	const bool bHasTag = IsValid(ASC) && Data.Tag.IsValid() && ASC->HasMatchingGameplayTag(Data.Tag);
	return Data.bInvert ? !bHasTag : bHasTag;
}

// ---------------------------------------------------------------- 추적

EStateTreeRunStatus FODStateTreeChaseTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	AAIController* Controller = ODStateTreeNodes::GetController(Enemy);
	AActor* Target = Enemy ? Enemy->UpdateCombatTarget() : nullptr;
	if (!Controller || !IsValid(Target))
	{
		return EStateTreeRunStatus::Failed;
	}
	if (ODStateTreeNodes::DistanceToTarget2D(Enemy, Target) <= Data.AcceptanceRadius)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// 사거리보다 조금 안쪽까지 들어가게 요청한다. (도착 판정은 Tick의 중심 간 거리 검사)
	// bStopOnOverlap을 끈다: 켜면 캡슐이 닿는 순간 멈춰, 몸집이 큰 적(보스)은 중심 거리가 사거리 안으로 들어오지 못한다.
	const EPathFollowingRequestResult::Type Result = Controller->MoveToActor(Target, FMath::Max(Data.AcceptanceRadius - 40.0f, 10.0f), false, true, false);
	return Result == EPathFollowingRequestResult::Failed ? EStateTreeRunStatus::Failed : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FODStateTreeChaseTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	AAIController* Controller = ODStateTreeNodes::GetController(Enemy);
	AActor* Target = Enemy ? Enemy->UpdateCombatTarget() : nullptr;
	if (!Controller || !IsValid(Target))
	{
		return EStateTreeRunStatus::Failed;
	}
	if (ODStateTreeNodes::DistanceToTarget2D(Enemy, Target) <= Data.AcceptanceRadius)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// 길찾기가 끝났는데 아직 멀면(대상이 움직였거나 막힘) 다시 요청한다.
	if (Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		if (Controller->MoveToActor(Target, FMath::Max(Data.AcceptanceRadius - 40.0f, 10.0f), false, true, false) == EPathFollowingRequestResult::Failed)
		{
			return EStateTreeRunStatus::Failed;
		}
	}
	return EStateTreeRunStatus::Running;
}

void FODStateTreeChaseTargetTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (AAIController* Controller = ODStateTreeNodes::GetController(Cast<AODEnemyCharacter>(Data.Actor)))
	{
		Controller->StopMovement();
	}
}

// ---------------------------------------------------------------- 귀환·대기

EStateTreeRunStatus FODStateTreeReturnHomeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.IdleElapsed = 0.0f;

	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	AAIController* Controller = ODStateTreeNodes::GetController(Enemy);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (FVector::Dist2D(Enemy->GetActorLocation(), Enemy->GetSpawnLocation()) > Data.AcceptanceRadius)
	{
		Controller->MoveToLocation(Enemy->GetSpawnLocation(), FMath::Max(Data.AcceptanceRadius - 30.0f, 10.0f));
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FODStateTreeReturnHomeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	AAIController* Controller = ODStateTreeNodes::GetController(Enemy);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 대상이 생기면 곧바로 끝내 상위 상태가 추적·공격을 고르게 한다.
	if (IsValid(Enemy->UpdateCombatTarget()))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const bool bAtHome = FVector::Dist2D(Enemy->GetActorLocation(), Enemy->GetSpawnLocation()) <= Data.AcceptanceRadius;
	if (!bAtHome)
	{
		if (Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			Controller->MoveToLocation(Enemy->GetSpawnLocation(), FMath::Max(Data.AcceptanceRadius - 30.0f, 10.0f));
		}
		return EStateTreeRunStatus::Running;
	}

	Data.IdleElapsed += DeltaTime;
	return Data.IdleElapsed >= Data.IdleTime ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FODStateTreeReturnHomeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (AAIController* Controller = ODStateTreeNodes::GetController(Cast<AODEnemyCharacter>(Data.Actor)))
	{
		Controller->StopMovement();
	}
}

// ---------------------------------------------------------------- 어빌리티 발동

EStateTreeRunStatus FODStateTreeActivateAbilityTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Presses = 0;
	Data.Elapsed = 0.0f;
	Data.SinceLastPress = 0.0f;

	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	UODAbilitySystemComponent* ASC = Enemy ? Enemy->GetODAbilitySystemComponent() : nullptr;
	if (!IsValid(ASC) || !Data.InputTag.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	// 이동을 멈추고 대상 쪽으로 돌아선 뒤 발동한다. (어빌리티도 GetAimDirection으로 대상을 향한다)
	if (AAIController* Controller = ODStateTreeNodes::GetController(Enemy))
	{
		Controller->StopMovement();
	}
	if (const AActor* Target = Enemy->UpdateCombatTarget(); IsValid(Target))
	{
		ODStateTreeNodes::FaceTarget(Enemy, Target);
	}

	if (!ASC->TryActivateAbilityByInputTag(Data.InputTag))
	{
		UE_LOG(LogODAI, Verbose, TEXT("%s 어빌리티 발동 실패: %s"), *GetNameSafe(Enemy), *Data.InputTag.ToString());
		return EStateTreeRunStatus::Failed;
	}
	Data.Presses = 1;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FODStateTreeActivateAbilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	UODAbilitySystemComponent* ASC = Enemy ? Enemy->GetODAbilitySystemComponent() : nullptr;
	if (!IsValid(ASC))
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.Elapsed += DeltaTime;
	Data.SinceLastPress += DeltaTime;

	// 어빌리티가 끝나면(콤보 끝, 경직으로 끊김 포함) 완료
	if (!ASC->IsAbilityActiveByInputTag(Data.InputTag))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	if (Data.Elapsed >= Data.Timeout)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 콤보 이어 치기: 실행 중인 콤보 어빌리티에 입력을 넣으면 다음 타격이 예약된다.
	if (Data.Presses < Data.PressCount && Data.SinceLastPress >= Data.PressInterval)
	{
		ASC->AbilityInputTagPressed(Data.InputTag);
		ASC->AbilityInputTagReleased(Data.InputTag);
		++Data.Presses;
		Data.SinceLastPress = 0.0f;
	}
	return EStateTreeRunStatus::Running;
}

// ---------------------------------------------------------------- 에디터 설명

#if WITH_EDITOR
FText FODStateTreeHasTargetCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(Data && Data->bInvert ? TEXT("대상 없음") : TEXT("대상 있음"));
}

FText FODStateTreeTargetInRangeCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(FString::Printf(TEXT("대상 %s %.0fcm"), Data && Data->bInvert ? TEXT("밖") : TEXT("안"), Data ? Data->MaxRange : 0.0f));
}

FText FODStateTreeHasTagCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(FString::Printf(TEXT("%s태그 %s"), Data && Data->bInvert ? TEXT("없음: ") : TEXT(""), Data ? *Data->Tag.ToString() : TEXT("")));
}

FText FODStateTreeChaseTargetTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(FString::Printf(TEXT("대상 추적 (%.0fcm)"), Data ? Data->AcceptanceRadius : 0.0f));
}

FText FODStateTreeReturnHomeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("제자리 복귀·대기"));
}

FText FODStateTreeActivateAbilityTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(FString::Printf(TEXT("발동 %s x%d"), Data ? *Data->InputTag.ToString() : TEXT(""), Data ? Data->PressCount : 0));
}
#endif
