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
	// 초기화 도중 등록이 바뀌어도 안전하게 복사본으로 돈다.
	const TArray<AODEnemyCharacter*> Snapshot = GetEnemies();
	for (AODEnemyCharacter* Enemy : Snapshot)
	{
		Enemy->ResetToSpawn();
	}
	UE_LOG(LogOD, Log, TEXT("적 전체 초기화: %d"), Snapshot.Num());
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
