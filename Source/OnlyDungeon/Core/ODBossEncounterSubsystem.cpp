// OnlyDungeon 보스전 서브시스템

#include "ODBossEncounterSubsystem.h"

#include "Character/ODEnemyCharacter.h"
#include "Core/ODLogChannels.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
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
	if (bVictory)
	{
		PlayVictorySlowMotion();
	}
	else
	{
		RestoreTimeDilation();
	}
	OnEncounterEnded.Broadcast(bVictory);
	Entrance.Reset();
}

void UODBossEncounterSubsystem::PlayVictorySlowMotion()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	// 마지막 일격의 순간을 느리게 보여 준다. 타이머도 느려진 시간으로 흐르므로 실제 시간 = 대기 시간 / 배율.
	constexpr float Dilation = 0.3f;
	constexpr float RealDuration = 1.2f;
	UGameplayStatics::SetGlobalTimeDilation(World, Dilation);
	World->GetTimerManager().SetTimer(SlowMotionTimer, this, &ThisClass::RestoreTimeDilation, RealDuration * Dilation, false);
}

void UODBossEncounterSubsystem::RestoreTimeDilation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SlowMotionTimer);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
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
