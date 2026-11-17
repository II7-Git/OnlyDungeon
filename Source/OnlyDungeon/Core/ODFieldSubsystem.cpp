// OnlyDungeon 필드 서브시스템

#include "ODFieldSubsystem.h"

#include "Character/ODEnemyCharacter.h"
#include "Core/ODLogChannels.h"

bool UODFieldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UODFieldSubsystem::RegisterEnemy(AODEnemyCharacter* Enemy)
{
	if (IsValid(Enemy))
	{
		Enemies.AddUnique(Enemy);
	}
}

void UODFieldSubsystem::UnregisterEnemy(AODEnemyCharacter* Enemy)
{
	Enemies.Remove(Enemy);
}

void UODFieldSubsystem::ResetAllEnemies()
{
	AttackTokenHolders.Reset();

	// 초기화 도중 등록이 바뀌어도 안전하게 복사본으로 돈다.
	const TArray<AODEnemyCharacter*> Snapshot = GetEnemies();
	for (AODEnemyCharacter* Enemy : Snapshot)
	{
		Enemy->ResetToSpawn();
	}
	UE_LOG(LogOD, Log, TEXT("적 전체 초기화: %d"), Snapshot.Num());
}

void UODFieldSubsystem::AlertNearbyEnemies(AODEnemyCharacter* Source, AODCharacterBase* Target, float Radius)
{
	if (!IsValid(Source) || !IsValid(Target) || Radius <= 0.0f)
	{
		return;
	}

	int32 Alerted = 0;
	const FVector Origin = Source->GetActorLocation();
	for (AODEnemyCharacter* Enemy : GetEnemies())
	{
		if (Enemy != Source && FVector::DistSquared(Origin, Enemy->GetActorLocation()) <= FMath::Square(Radius) && Enemy->ReceiveAlert(Target))
		{
			++Alerted;
		}
	}
	if (Alerted > 0)
	{
		UE_LOG(LogODAI, Log, TEXT("%s 발견 → 주변 %d마리 함께 어그로"), *GetNameSafe(Source), Alerted);
	}
}

void UODFieldSubsystem::PruneAttackTokens() const
{
	AttackTokenHolders.RemoveAll([](const TWeakObjectPtr<AODEnemyCharacter>& Holder)
	{
		const AODEnemyCharacter* Enemy = Holder.Get();
		return !IsValid(Enemy) || Enemy->IsDead();
	});
}

bool UODFieldSubsystem::CanAcquireAttackToken(const AODEnemyCharacter* Enemy) const
{
	PruneAttackTokens();
	return AttackTokenHolders.Contains(Enemy) || AttackTokenHolders.Num() < MaxAttackTokens;
}

bool UODFieldSubsystem::AcquireAttackToken(AODEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy) || !CanAcquireAttackToken(Enemy))
	{
		return false;
	}
	AttackTokenHolders.AddUnique(Enemy);
	return true;
}

void UODFieldSubsystem::ReleaseAttackToken(AODEnemyCharacter* Enemy)
{
	AttackTokenHolders.Remove(Enemy);
}

TArray<AODEnemyCharacter*> UODFieldSubsystem::GetEnemies() const
{
	TArray<AODEnemyCharacter*> Result;
	Result.Reserve(Enemies.Num());
	for (const TWeakObjectPtr<AODEnemyCharacter>& WeakEnemy : Enemies)
	{
		if (AODEnemyCharacter* Enemy = WeakEnemy.Get(); IsValid(Enemy))
		{
			Result.Add(Enemy);
		}
	}
	return Result;
}
