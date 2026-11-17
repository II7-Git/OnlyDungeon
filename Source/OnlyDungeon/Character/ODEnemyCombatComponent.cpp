// OnlyDungeon 적 전투 컴포넌트

#include "ODEnemyCombatComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AI/ODCombatPatternSet.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Character/ODEnemyCharacter.h"
#include "Core/ODFieldSubsystem.h"
#include "Core/ODLogChannels.h"
#include "Engine/World.h"

UODEnemyCombatComponent::UODEnemyCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UODEnemyCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseToken();
	Super::EndPlay(EndPlayReason);
}

AODEnemyCharacter* UODEnemyCombatComponent::GetEnemy() const
{
	return Cast<AODEnemyCharacter>(GetOwner());
}

double UODEnemyCombatComponent::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

bool UODEnemyCombatComponent::IsAttackReady() const
{
	if (!IsValid(PatternSet) || GetNow() < NextAttackTime)
	{
		return false;
	}
	if (PatternSet->bUseAttackToken && !bHoldingToken)
	{
		const UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr;
		return !Field || Field->CanAcquireAttackToken(GetEnemy());
	}
	return true;
}

const FODCombatPattern* UODEnemyCombatComponent::GetPattern(int32 Index) const
{
	return IsValid(PatternSet) && PatternSet->Patterns.IsValidIndex(Index) ? &PatternSet->Patterns[Index] : nullptr;
}

bool UODEnemyCombatComponent::IsPatternUsable(const FODCombatPattern& Pattern, int32 Index, float Distance, bool bPunish, float ApproachSlack) const
{
	if (!Pattern.AbilityClass || Pattern.Weight <= 0.0f)
	{
		return false;
	}
	if (bPunish ? !Pattern.bPunish : Pattern.bPunishOnly)
	{
		return false;
	}
	if (PatternReadyTime.IsValidIndex(Index) && GetNow() < PatternReadyTime[Index])
	{
		return false;
	}
	if (Distance < Pattern.MinRange || Distance > Pattern.MaxRange + ApproachSlack)
	{
		return false;
	}
	if (!Pattern.RequiredTags.IsEmpty())
	{
		const AODEnemyCharacter* Enemy = GetEnemy();
		const UAbilitySystemComponent* ASC = IsValid(Enemy) ? Enemy->GetODAbilitySystemComponent() : nullptr;
		if (!IsValid(ASC) || !ASC->HasAllMatchingGameplayTags(Pattern.RequiredTags))
		{
			return false;
		}
	}
	return true;
}

int32 UODEnemyCombatComponent::SelectPattern(float Distance, bool bPunish, float ApproachSlack) const
{
	if (!IsValid(PatternSet))
	{
		return INDEX_NONE;
	}

	float TotalWeight = 0.0f;
	TArray<int32, TInlineAllocator<8>> Candidates;
	for (int32 Index = 0; Index < PatternSet->Patterns.Num(); ++Index)
	{
		const FODCombatPattern& Pattern = PatternSet->Patterns[Index];
		if (IsPatternUsable(Pattern, Index, Distance, bPunish, ApproachSlack))
		{
			Candidates.Add(Index);
			TotalWeight += Pattern.Weight;
		}
	}
	if (Candidates.IsEmpty())
	{
		return INDEX_NONE;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const int32 Index : Candidates)
	{
		Roll -= PatternSet->Patterns[Index].Weight;
		if (Roll <= 0.0f)
		{
			return Index;
		}
	}
	return Candidates.Last();
}

void UODEnemyCombatComponent::NotifyPatternStarted(int32 Index)
{
	const FODCombatPattern* Pattern = GetPattern(Index);
	if (!Pattern)
	{
		return;
	}

	if (PatternReadyTime.Num() != PatternSet->Patterns.Num())
	{
		PatternReadyTime.SetNumZeroed(PatternSet->Patterns.Num());
	}
	PatternReadyTime[Index] = GetNow() + Pattern->Cooldown;

	// 공격 중에는 다음 공격을 시작하지 않는다. (끝날 때 공통 간격으로 다시 맞춘다)
	NextAttackTime = TNumericLimits<double>::Max();

	if (PatternSet->bUseAttackToken && !bHoldingToken)
	{
		if (UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr)
		{
			bHoldingToken = Field->AcquireAttackToken(GetEnemy());
		}
	}

	// 빈틈 공략을 시작했으면 이번 빈틈은 끝난 것으로 본다.
	if (Pattern->bPunish)
	{
		bPunishThisOpening = false;
	}

	UE_LOG(LogODAI, Log, TEXT("%s 패턴 시작: %s"), *GetNameSafe(GetOwner()), *Pattern->Name.ToString());
}

void UODEnemyCombatComponent::NotifyPatternFinished(int32 Index)
{
	if (!IsValid(PatternSet))
	{
		return;
	}

	float Interval = FMath::FRandRange(PatternSet->AttackInterval.X, FMath::Max(PatternSet->AttackInterval.X, PatternSet->AttackInterval.Y));
	if (PatternSet->AggressiveTag.IsValid())
	{
		const AODEnemyCharacter* Enemy = GetEnemy();
		const UAbilitySystemComponent* ASC = IsValid(Enemy) ? Enemy->GetODAbilitySystemComponent() : nullptr;
		if (IsValid(ASC) && ASC->HasMatchingGameplayTag(PatternSet->AggressiveTag))
		{
			Interval *= PatternSet->AggressiveIntervalScale;
		}
	}
	if (const FODCombatPattern* Pattern = GetPattern(Index))
	{
		Interval += Pattern->ExtraRecovery;
	}
	NextAttackTime = GetNow() + Interval;
	ReleaseToken();

	UE_LOG(LogODAI, Verbose, TEXT("%s 다음 공격까지 %.2f초"), *GetNameSafe(GetOwner()), Interval);
}

bool UODEnemyCombatComponent::IsTargetInOpening(const AActor* Target) const
{
	if (!IsValid(PatternSet) || PatternSet->OpeningTags.IsEmpty() || !IsValid(Target))
	{
		return false;
	}
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	return IsValid(TargetASC) && TargetASC->HasAnyMatchingGameplayTags(PatternSet->OpeningTags);
}

bool UODEnemyCombatComponent::ShouldPunish(const AActor* Target, float Distance, float ApproachSlack) const
{
	const bool bInOpening = IsTargetInOpening(Target);
	if (bInOpening && !bWasInOpening)
	{
		// 빈틈이 새로 생겼다: 이번 빈틈을 공략할지 한 번만 정한다.
		bPunishThisOpening = FMath::FRand() < PatternSet->PunishChance;
	}
	bWasInOpening = bInOpening;

	// 공격 순번을 쓰는 몬스터는 빈틈 공략도 순번을 얻을 수 있을 때만 한다. (여럿이 한꺼번에 파고드는 것 방지)
	if (PatternSet->bUseAttackToken && !bHoldingToken)
	{
		const UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr;
		if (Field && !Field->CanAcquireAttackToken(GetEnemy()))
		{
			return false;
		}
	}

	return bInOpening && bPunishThisOpening && SelectPattern(Distance, true, ApproachSlack) != INDEX_NONE;
}

void UODEnemyCombatComponent::ResetCombatState()
{
	PatternReadyTime.Reset();
	NextAttackTime = 0.0;
	bWasInOpening = false;
	bPunishThisOpening = false;
	ReleaseToken();
}

void UODEnemyCombatComponent::ReleaseToken()
{
	if (!bHoldingToken)
	{
		return;
	}
	bHoldingToken = false;
	if (UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr)
	{
		Field->ReleaseAttackToken(GetEnemy());
	}
}
