// OnlyDungeon 적 AI용 StateTree 노드

#include "ODStateTreeNodes.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/ODCombatPatternSet.h"
#include "Character/ODEnemyCharacter.h"
#include "Character/ODEnemyCombatComponent.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODFieldSubsystem.h"
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

// ---------------------------------------------------------------- 패턴 전투

namespace ODStateTreeNodes
{
	UODEnemyCombatComponent* GetCombat(const AODEnemyCharacter* Enemy)
	{
		return IsValid(Enemy) ? Enemy->GetCombatComponent() : nullptr;
	}

	/** 패턴 어빌리티의 스펙 */
	FGameplayAbilitySpec* FindPatternSpec(UODAbilitySystemComponent* ASC, const FODCombatPattern& Pattern)
	{
		return IsValid(ASC) && Pattern.AbilityClass ? ASC->FindAbilitySpecFromClass(Pattern.AbilityClass) : nullptr;
	}

	/** 스펙에 연결된 입력 태그 (콤보 이어 치기 입력용) */
	FGameplayTag FindInputTag(const FGameplayAbilitySpec& Spec)
	{
		const FGameplayTag InputRoot = FGameplayTag::RequestGameplayTag(TEXT("InputTag"), false);
		for (const FGameplayTag& Tag : Spec.GetDynamicSpecSourceTags())
		{
			if (InputRoot.IsValid() && Tag.MatchesTag(InputRoot))
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}

	/** 패턴 어빌리티를 발동한다. */
	bool ActivatePattern(FODStateTreeExecutePatternTaskInstanceData& Data, AODEnemyCharacter* Enemy)
	{
		UODEnemyCombatComponent* Combat = GetCombat(Enemy);
		const FODCombatPattern* Pattern = Combat ? Combat->GetPattern(Data.PatternIndex) : nullptr;
		UODAbilitySystemComponent* ASC = Enemy->GetODAbilitySystemComponent();
		if (!Pattern || !IsValid(ASC))
		{
			return false;
		}
		if (AAIController* Controller = GetController(Enemy))
		{
			Controller->StopMovement();
		}
		if (!ASC->TryActivateAbilityByClass(Pattern->AbilityClass))
		{
			UE_LOG(LogODAI, Verbose, TEXT("%s 패턴 발동 실패: %s"), *GetNameSafe(Enemy), *Pattern->Name.ToString());
			return false;
		}
		Combat->NotifyPatternStarted(Data.PatternIndex);
		Data.bStarted = true;
		Data.Presses = 1;
		Data.SinceLastPress = 0.0f;
		return true;
	}
}

bool FODStateTreeCanAttackCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	const AActor* Target = Enemy ? Enemy->UpdateCombatTarget() : nullptr;
	const UODEnemyCombatComponent* Combat = ODStateTreeNodes::GetCombat(Enemy);
	const bool bCanAttack = IsValid(Target) && Combat && Combat->IsAttackReady()
		&& Combat->SelectPattern(ODStateTreeNodes::DistanceToTarget2D(Enemy, Target), false, Data.ApproachSlack) != INDEX_NONE;
	return Data.bInvert ? !bCanAttack : bCanAttack;
}

bool FODStateTreeTargetOpeningCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	const AActor* Target = Enemy ? Enemy->UpdateCombatTarget() : nullptr;
	const UODEnemyCombatComponent* Combat = ODStateTreeNodes::GetCombat(Enemy);
	// 빈틈 공략은 공통 간격을 무시하지만, 순번을 쓰는 몬스터는 순번이 있어야 한다. (IsAttackReady의 간격 조건은 보지 않는다)
	const bool bOpening = IsValid(Target) && Combat && Combat->ShouldPunish(Target, ODStateTreeNodes::DistanceToTarget2D(Enemy, Target), Data.ApproachSlack);
	return Data.bInvert ? !bOpening : bOpening;
}

EStateTreeRunStatus FODStateTreeExecutePatternTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.PatternIndex = INDEX_NONE;
	Data.bStarted = false;
	Data.Presses = 0;
	Data.Elapsed = 0.0f;
	Data.SinceLastPress = 0.0f;

	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	UODEnemyCombatComponent* Combat = ODStateTreeNodes::GetCombat(Enemy);
	AActor* Target = Enemy ? Enemy->UpdateCombatTarget() : nullptr;
	AAIController* Controller = ODStateTreeNodes::GetController(Enemy);
	if (!Combat || !IsValid(Target) || !Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	const float Distance = ODStateTreeNodes::DistanceToTarget2D(Enemy, Target);
	Data.PatternIndex = Combat->SelectPattern(Distance, Data.bPunish, Data.ApproachSlack);
	const FODCombatPattern* Pattern = Combat->GetPattern(Data.PatternIndex);
	if (!Pattern)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 공격 순번은 다가가기 시작할 때 미리 잡는다. (도착해서 잡으면 접근 중인 몬스터가 모두 순번이 남았다고 보고 한꺼번에 달려든다)
	if (Combat->GetPatternSet()->bUseAttackToken)
	{
		UODFieldSubsystem* Field = Enemy->GetWorld() ? Enemy->GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr;
		if (Field && !Field->AcquireAttackToken(Enemy))
		{
			Data.PatternIndex = INDEX_NONE;
			return EStateTreeRunStatus::Failed;
		}
	}

	if (Distance <= Pattern->MaxRange)
	{
		return ODStateTreeNodes::ActivatePattern(Data, Enemy) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}

	// 사거리 밖: 다가간다. (bStopOnOverlap 끔: 큰 적도 중심 거리가 사거리 안으로 들어오게)
	const EPathFollowingRequestResult::Type Result = Controller->MoveToActor(Target, FMath::Max(Pattern->MaxRange - 50.0f, 10.0f), false, true, false);
	return Result == EPathFollowingRequestResult::Failed ? EStateTreeRunStatus::Failed : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FODStateTreeExecutePatternTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	UODEnemyCombatComponent* Combat = ODStateTreeNodes::GetCombat(Enemy);
	const FODCombatPattern* Pattern = Combat ? Combat->GetPattern(Data.PatternIndex) : nullptr;
	UODAbilitySystemComponent* ASC = Enemy ? Enemy->GetODAbilitySystemComponent() : nullptr;
	if (!Pattern || !IsValid(ASC))
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.Elapsed += DeltaTime;

	// 접근 중
	if (!Data.bStarted)
	{
		AActor* Target = Enemy->UpdateCombatTarget();
		AAIController* Controller = ODStateTreeNodes::GetController(Enemy);
		if (!IsValid(Target) || !Controller || Data.Elapsed >= Data.ApproachTimeout)
		{
			return EStateTreeRunStatus::Failed;
		}
		if (ODStateTreeNodes::DistanceToTarget2D(Enemy, Target) <= Pattern->MaxRange)
		{
			return ODStateTreeNodes::ActivatePattern(Data, Enemy) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}
		if (Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			Controller->MoveToActor(Target, FMath::Max(Pattern->MaxRange - 50.0f, 10.0f), false, true, false);
		}
		return EStateTreeRunStatus::Running;
	}

	// 실행 중: 어빌리티가 끝나면(경직으로 끊김 포함) 완료
	FGameplayAbilitySpec* Spec = ODStateTreeNodes::FindPatternSpec(ASC, *Pattern);
	if (!Spec || !Spec->IsActive())
	{
		return EStateTreeRunStatus::Succeeded;
	}
	if (Data.Elapsed >= Data.Timeout)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 콤보 이어 치기
	Data.SinceLastPress += DeltaTime;
	if (Data.Presses < Pattern->PressCount && Data.SinceLastPress >= Pattern->PressInterval)
	{
		if (const FGameplayTag InputTag = ODStateTreeNodes::FindInputTag(*Spec); InputTag.IsValid())
		{
			ASC->AbilityInputTagPressed(InputTag);
			ASC->AbilityInputTagReleased(InputTag);
		}
		++Data.Presses;
		Data.SinceLastPress = 0.0f;
	}
	return EStateTreeRunStatus::Running;
}

void FODStateTreeExecutePatternTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = Cast<AODEnemyCharacter>(Data.Actor);
	if (AAIController* Controller = ODStateTreeNodes::GetController(Enemy))
	{
		Controller->StopMovement();
	}
	// 공격했든 다가가다 포기했든 공통 간격을 시작한다. (같은 자리에서 매 틱 재시도하지 않게)
	if (UODEnemyCombatComponent* Combat = ODStateTreeNodes::GetCombat(Enemy); Combat && Data.PatternIndex != INDEX_NONE)
	{
		Combat->NotifyPatternFinished(Data.bStarted ? Data.PatternIndex : INDEX_NONE);

		// 다가가다 포기했으면 미리 잡아 둔 순번을 돌려준다. (발동했으면 컴포넌트가 돌려준다)
		if (!Data.bStarted && IsValid(Enemy) && IsValid(Combat->GetPatternSet()) && Combat->GetPatternSet()->bUseAttackToken)
		{
			if (UODFieldSubsystem* Field = Enemy->GetWorld() ? Enemy->GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr)
			{
				Field->ReleaseAttackToken(Enemy);
			}
		}
	}
	Data.PatternIndex = INDEX_NONE;
	Data.bStarted = false;
}

// ---------------------------------------------------------------- 간 보기

EStateTreeRunStatus FODStateTreeStrafeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.Elapsed = 0.0f;
	Data.StuckTime = 0.0f;
	Data.SavedMaxWalkSpeed = -1.0f;

	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	const UODEnemyCombatComponent* Combat = ODStateTreeNodes::GetCombat(Enemy);
	const UODCombatPatternSet* Set = Combat ? Combat->GetPatternSet() : nullptr;
	AAIController* Controller = ODStateTreeNodes::GetController(Enemy);
	AActor* Target = Enemy ? Enemy->UpdateCombatTarget() : nullptr;
	UCharacterMovementComponent* Movement = Enemy ? Enemy->GetCharacterMovement() : nullptr;
	if (!Set || !Controller || !IsValid(Target) || !IsValid(Movement))
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.Duration = FMath::FRandRange(Set->StrafeDuration.X, FMath::Max(Set->StrafeDuration.X, Set->StrafeDuration.Y));
	Data.Direction = FMath::RandBool() ? 1.0f : -1.0f;

	// 대상을 바라본 채 옆걸음: 이동 방향으로 돌지 않고 컨트롤러(시선 고정) 방향을 따른다.
	Data.bSavedOrientToMovement = Movement->bOrientRotationToMovement;
	Data.bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
	Data.SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	Movement->MaxWalkSpeed = Set->StrafeSpeed;
	Controller->StopMovement();
	Controller->SetFocus(Target);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FODStateTreeStrafeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	UODEnemyCombatComponent* Combat = ODStateTreeNodes::GetCombat(Enemy);
	const UODCombatPatternSet* Set = Combat ? Combat->GetPatternSet() : nullptr;
	AActor* Target = Enemy ? Enemy->UpdateCombatTarget() : nullptr;
	if (!Set || !IsValid(Target))
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.Elapsed += DeltaTime;
	const float Distance = ODStateTreeNodes::DistanceToTarget2D(Enemy, Target);
	if (Data.Elapsed >= Data.Duration)
	{
		return EStateTreeRunStatus::Succeeded;
	}
	if (Data.Elapsed >= Data.MinDuration && (Combat->ShouldPunish(Target, Distance, 300.0f) || Combat->IsAttackReady()))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// 경직·공격 중(피격 몽타주 등)에는 움직이지 않는다.
	const UODAbilitySystemComponent* ASC = Enemy->GetODAbilitySystemComponent();
	if (IsValid(ASC) && (ASC->HasMatchingGameplayTag(ODGameplayTags::State_Stunned) || ASC->HasMatchingGameplayTag(ODGameplayTags::State_Attacking)))
	{
		return EStateTreeRunStatus::Running;
	}

	// 대상 둘레를 도는 방향 + 유지 거리로 돌아가는 방향
	FVector ToTarget = Target->GetActorLocation() - Enemy->GetActorLocation();
	ToTarget.Z = 0.0;
	const FVector Radial = ToTarget.GetSafeNormal();
	const FVector Tangent = FVector::CrossProduct(FVector::UpVector, Radial) * Data.Direction;
	const float DistanceError = Distance - Set->StrafeDistance;
	const float RadialWeight = FMath::Clamp(DistanceError / 150.0f, -1.0f, 1.0f);
	const FVector MoveDirection = (Tangent + Radial * RadialWeight).GetSafeNormal();
	Enemy->AddMovementInput(MoveDirection, 1.0f);

	// 벽 등에 막혀 거의 못 움직이면 반대로 돈다.
	if (Enemy->GetVelocity().Size2D() < Set->StrafeSpeed * 0.2f)
	{
		Data.StuckTime += DeltaTime;
		if (Data.StuckTime > 0.4f)
		{
			Data.Direction *= -1.0f;
			Data.StuckTime = 0.0f;
		}
	}
	else
	{
		Data.StuckTime = 0.0f;
	}
	return EStateTreeRunStatus::Running;
}

void FODStateTreeStrafeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = Cast<AODEnemyCharacter>(Data.Actor);
	if (AAIController* Controller = ODStateTreeNodes::GetController(Enemy))
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
	}
	if (UCharacterMovementComponent* Movement = IsValid(Enemy) ? Enemy->GetCharacterMovement() : nullptr; IsValid(Movement) && Data.SavedMaxWalkSpeed >= 0.0f)
	{
		Movement->bOrientRotationToMovement = Data.bSavedOrientToMovement;
		Movement->bUseControllerDesiredRotation = Data.bSavedUseControllerDesiredRotation;
		Movement->MaxWalkSpeed = Data.SavedMaxWalkSpeed;
	}
	Data.SavedMaxWalkSpeed = -1.0f;
}

// ---------------------------------------------------------------- 발견 전 행동

namespace ODStateTreeNodes
{
	void StopAmbushPose(AODEnemyCharacter* Enemy, FODStateTreeIdleBehaviorTaskInstanceData& Data)
	{
		if (!Data.bLying)
		{
			return;
		}
		Data.bLying = false;
		if (UAnimInstance* Anim = IsValid(Enemy) && IsValid(Enemy->GetMesh()) ? Enemy->GetMesh()->GetAnimInstance() : nullptr)
		{
			Anim->Montage_Stop(0.2f, Enemy->GetAmbushLoopMontage());
		}
	}
}

EStateTreeRunStatus FODStateTreeIdleBehaviorTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.WaitElapsed = 0.0f;
	Data.bLying = false;
	Data.bGettingUp = false;
	return ODStateTreeNodes::GetController(ODStateTreeNodes::GetEnemy(Data.Actor)) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FODStateTreeIdleBehaviorTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = ODStateTreeNodes::GetEnemy(Data.Actor);
	AAIController* Controller = ODStateTreeNodes::GetController(Enemy);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}
	UAnimInstance* Anim = IsValid(Enemy->GetMesh()) ? Enemy->GetMesh()->GetAnimInstance() : nullptr;

	// 일어나는 동작이 끝나면 싸우러 간다.
	if (Data.bGettingUp)
	{
		return (Anim && Anim->Montage_IsPlaying(Enemy->GetAmbushGetUpMontage())) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
	}

	// 대상 발견
	if (IsValid(Enemy->UpdateCombatTarget()))
	{
		Controller->StopMovement();
		if (Data.bLying && Anim && IsValid(Enemy->GetAmbushGetUpMontage()))
		{
			ODStateTreeNodes::StopAmbushPose(Enemy, Data);
			Anim->Montage_Play(Enemy->GetAmbushGetUpMontage());
			Data.bGettingUp = true;
			return EStateTreeRunStatus::Running;
		}
		ODStateTreeNodes::StopAmbushPose(Enemy, Data);
		return EStateTreeRunStatus::Succeeded;
	}

	const auto MoveTo = [&](const FVector& Goal) -> bool
	{
		if (FVector::Dist2D(Enemy->GetActorLocation(), Goal) <= Data.AcceptanceRadius)
		{
			return true;
		}
		if (Controller->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			Controller->MoveToLocation(Goal, FMath::Max(Data.AcceptanceRadius - 30.0f, 10.0f));
		}
		return false;
	};

	EODEnemyIdleBehavior Behavior = Enemy->GetIdleBehavior();
	if (Behavior == EODEnemyIdleBehavior::Patrol && Enemy->GetNumPatrolPoints() == 0)
	{
		Behavior = EODEnemyIdleBehavior::Stand;
	}
	if (Behavior == EODEnemyIdleBehavior::Ambush && (!IsValid(Enemy->GetAmbushLoopMontage()) || !Anim))
	{
		Behavior = EODEnemyIdleBehavior::Stand;
	}

	switch (Behavior)
	{
	case EODEnemyIdleBehavior::Patrol:
	{
		if (MoveTo(Enemy->GetPatrolPoint(Data.PatrolIndex)))
		{
			Data.WaitElapsed += DeltaTime;
			if (Data.WaitElapsed >= Enemy->GetPatrolWaitTime())
			{
				Data.WaitElapsed = 0.0f;
				Data.PatrolIndex = (Data.PatrolIndex + 1) % Enemy->GetNumPatrolPoints();
			}
		}
		break;
	}
	case EODEnemyIdleBehavior::Ambush:
	{
		// 시작 위치에 돌아간 뒤 다시 쓰러진 자세로 기다린다.
		if (MoveTo(Enemy->GetSpawnLocation()) && !Data.bLying)
		{
			Anim->Montage_Play(Enemy->GetAmbushLoopMontage());
			Data.bLying = true;
		}
		break;
	}
	default:
		MoveTo(Enemy->GetSpawnLocation());
		break;
	}
	return EStateTreeRunStatus::Running;
}

void FODStateTreeIdleBehaviorTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	AODEnemyCharacter* Enemy = Cast<AODEnemyCharacter>(Data.Actor);
	if (AAIController* Controller = ODStateTreeNodes::GetController(Enemy))
	{
		Controller->StopMovement();
	}
	ODStateTreeNodes::StopAmbushPose(Enemy, Data);
	Data.bGettingUp = false;
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

FText FODStateTreeCanAttackCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(Data && Data->bInvert ? TEXT("공격 불가") : TEXT("공격 가능"));
}

FText FODStateTreeTargetOpeningCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(Data && Data->bInvert ? TEXT("빈틈 없음") : TEXT("대상 빈틈 공략"));
}

FText FODStateTreeExecutePatternTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* Data = InstanceDataView.GetPtr<FInstanceDataType>();
	return FText::FromString(Data && Data->bPunish ? TEXT("빈틈 공략 패턴 실행") : TEXT("패턴 실행"));
}

FText FODStateTreeStrafeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("간 보기 (주위 돌기)"));
}

FText FODStateTreeIdleBehaviorTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("발견 전 행동 (서 있기·순찰·매복)"));
}
#endif
