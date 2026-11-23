// OnlyDungeon 필드 서브시스템

#include "ODFieldSubsystem.h"

#include "Character/ODEnemyCharacter.h"
#include "Core/ODBossEncounterSubsystem.h"
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
	AttackTokenHolders.Remove(Enemy);
	PruneAttackTokens();
}

void UODFieldSubsystem::ResetAllEnemies()
{
	AttackTokenHolders.Reset();

	// 처치한 보스는 부활·휴식으로 되살리지 않는다. (기획서 2장)
	const UWorld* World = GetWorld();
	const UODBossEncounterSubsystem* Encounter = World ? World->GetSubsystem<UODBossEncounterSubsystem>() : nullptr;

	// 초기화 도중 등록이 바뀌어도 안전하게 복사본으로 돈다.
	const TArray<AODEnemyCharacter*> Snapshot = GetEnemies();
	int32 ResetCount = 0;
	for (AODEnemyCharacter* Enemy : Snapshot)
	{
		if (Encounter && Encounter->IsBossDefeated(Enemy))
		{
			continue;
		}
		Enemy->ResetToSpawn();
		++ResetCount;
	}
	UE_LOG(LogOD, Log, TEXT("적 전체 초기화: %d (처치한 보스 %d 제외)"), ResetCount, Snapshot.Num() - ResetCount);
}

void UODFieldSubsystem::AlertNearbyEnemies(AODEnemyCharacter* Source, AODCharacterBase* Target, float Radius)
{
	if (!IsValid(Source) || !IsValid(Target) || Radius <= 0.0f)
	{
		return;
	}

	int32 Alerted = 0;
	const FVector Origin = Source->GetActorLocation();
	const double RadiusSquared = FMath::Square(Radius);
	ForEachEnemy([&](AODEnemyCharacter& Enemy)
	{
		if (&Enemy != Source && FVector::DistSquared(Origin, Enemy.GetActorLocation()) <= RadiusSquared && Enemy.ReceiveAlert(Target))
		{
			++Alerted;
		}
	});
	if (Alerted > 0)
	{
		UE_LOG(LogODAI, Log, TEXT("%s 발견 → 주변 %d마리 함께 어그로"), *GetNameSafe(Source), Alerted);
	}
}

void UODFieldSubsystem::PruneAttackTokens()
{
	AttackTokenHolders.RemoveAll([](const TWeakObjectPtr<AODEnemyCharacter>& Holder)
	{
		const AODEnemyCharacter* Enemy = Holder.Get();
		return !IsValid(Enemy) || Enemy->IsDead();
	});
}

int32 UODFieldSubsystem::CountActiveTokenHolders() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AODEnemyCharacter>& Holder : AttackTokenHolders)
	{
		if (const AODEnemyCharacter* HolderEnemy = Holder.Get(); IsValid(HolderEnemy) && !HolderEnemy->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

bool UODFieldSubsystem::CanAcquireAttackToken(const AODEnemyCharacter* Enemy) const
{
	// 사망·EndPlay 시 순번은 반납되지만, 반납이 누락돼도 죽은 보유자는 세지 않아 순번이 막히지 않는다.
	return AttackTokenHolders.Contains(Enemy) || CountActiveTokenHolders() < MaxAttackTokens;
}

bool UODFieldSubsystem::AcquireAttackToken(AODEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy))
	{
		return false;
	}
	PruneAttackTokens();
	if (!CanAcquireAttackToken(Enemy))
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

void UODFieldSubsystem::ForEachEnemy(TFunctionRef<void(AODEnemyCharacter&)> Func) const
{
	for (const TWeakObjectPtr<AODEnemyCharacter>& WeakEnemy : Enemies)
	{
		if (AODEnemyCharacter* Enemy = WeakEnemy.Get(); IsValid(Enemy))
		{
			Func(*Enemy);
		}
	}
}
