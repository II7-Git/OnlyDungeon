// OnlyDungeon 보스전 서브시스템

#include "ODBossEncounterSubsystem.h"

#include "Character/ODEnemyCharacter.h"
#include "Core/ODLogChannels.h"
#include "World/ODBossEntrance.h"

bool UODBossEncounterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UODBossEncounterSubsystem::StartEncounter(const TArray<AODEnemyCharacter*>& InBosses, AODBossEntrance* InEntrance)
{
	if (bActive)
	{
		EndEncounter(false);
	}

	Bosses.Reset(InBosses.Num());
	for (AODEnemyCharacter* Boss : InBosses)
	{
		if (IsValid(Boss) && !Boss->IsDead())
		{
			Bosses.Emplace(Boss);
		}
	}
	if (Bosses.IsEmpty())
	{
		UE_LOG(LogOD, Warning, TEXT("보스전 시작 실패: 살아 있는 보스가 없음 (입구: %s)"), *GetNameSafe(InEntrance));
		return;
	}

	Entrance = InEntrance;
	bActive = true;

	for (const TWeakObjectPtr<AODEnemyCharacter>& WeakBoss : Bosses)
	{
		AODEnemyCharacter* Boss = WeakBoss.Get();
		Boss->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleBossDeath);
		Boss->ActivateCombat();
	}

	AODEnemyCharacter* MainBoss = GetCurrentBoss();
	UE_LOG(LogOD, Log, TEXT("보스전 시작: %s (보스 %d)"), *GetNameSafe(MainBoss), Bosses.Num());
	OnEncounterStarted.Broadcast(MainBoss);
}

void UODBossEncounterSubsystem::EndEncounter(bool bVictory)
{
	if (!bActive)
	{
		return;
	}
	bActive = false;

	for (const TWeakObjectPtr<AODEnemyCharacter>& WeakBoss : Bosses)
	{
		AODEnemyCharacter* Boss = WeakBoss.Get();
		if (!IsValid(Boss))
		{
			continue;
		}
		Boss->OnDeath.RemoveDynamic(this, &ThisClass::HandleBossDeath);
		if (!bVictory)
		{
			Boss->DeactivateCombat();
		}
	}
	Bosses.Reset();

	UE_LOG(LogOD, Log, TEXT("보스전 종료: %s"), bVictory ? TEXT("승리") : TEXT("패배"));
	OnEncounterEnded.Broadcast(bVictory);
	Entrance.Reset();
}

AODEnemyCharacter* UODBossEncounterSubsystem::GetCurrentBoss() const
{
	for (const TWeakObjectPtr<AODEnemyCharacter>& WeakBoss : Bosses)
	{
		AODEnemyCharacter* Boss = WeakBoss.Get();
		if (IsValid(Boss) && !Boss->IsDead())
		{
			return Boss;
		}
	}
	return nullptr;
}

void UODBossEncounterSubsystem::HandleBossDeath(AODCharacterBase* DeadCharacter)
{
	// 보스가 모두 죽어야 승리
	if (bActive && !IsValid(GetCurrentBoss()))
	{
		EndEncounter(true);
	}
}
