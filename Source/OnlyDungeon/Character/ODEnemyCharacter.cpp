// OnlyDungeon 적 캐릭터 기반

#include "ODEnemyCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "AI/ODAIController.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODFieldSubsystem.h"
#include "Core/ODLogChannels.h"
#include "Engine/World.h"

AODEnemyCharacter::AODEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 레벨에 배치되거나 스폰될 때 AI 컨트롤러가 자동으로 빙의한다.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AODAIController::StaticClass();
	TeamId = 1;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
}

void AODEnemyCharacter::BeginPlay()
{
	bCombatActive = !bStartDormant;
	SpawnTransform = GetActorTransform();
	Super::BeginPlay();

	if (UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr)
	{
		Field->RegisterEnemy(this);
	}

	if (UODAbilitySystemComponent* ASC = GetODAbilitySystemComponent(); IsValid(ASC))
	{
		// 적은 보스전 규칙(강공격·방어 허용)으로 싸운다.
		ASC->SetLooseGameplayTagCount(ODGameplayTags::Mode_Boss, 1);

		if (!Phases.IsEmpty())
		{
			HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UODAttributeSet_Health::GetHealthAttribute()).AddWeakLambda(this, [this](const FOnAttributeChangeData&)
			{
				RefreshPhase();
			});
		}
	}
	RefreshPhase();

	// 빙의가 BeginPlay보다 먼저 일어나므로, 상태가 준비된 지금 AI를 시작(대기 상태면 정지)한다.
	RefreshAI(true);
}

void AODEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UODAbilitySystemComponent* ASC = GetODAbilitySystemComponent(); IsValid(ASC) && HealthChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UODAttributeSet_Health::GetHealthAttribute()).Remove(HealthChangedHandle);
	}
	if (UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr)
	{
		Field->UnregisterEnemy(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AODEnemyCharacter::ResetToSpawn()
{
	Revive();
	TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.Rotator());
}

void AODEnemyCharacter::ActivateCombat()
{
	SetCombatActive(true);
}

void AODEnemyCharacter::DeactivateCombat()
{
	SetCombatActive(false);
}

void AODEnemyCharacter::SetCombatActive(bool bNewActive)
{
	if (bCombatActive == bNewActive)
	{
		return;
	}
	bCombatActive = bNewActive;

	UE_LOG(LogODAI, Log, TEXT("%s 전투 %s"), *GetNameSafe(this), bNewActive ? TEXT("활성") : TEXT("대기"));
	HandleCombatActiveChanged(bNewActive);
	OnCombatActiveChanged.Broadcast(this, bNewActive);
}

void AODEnemyCharacter::HandleCombatActiveChanged(bool bNewActive)
{
	if (!bNewActive)
	{
		CombatTarget.Reset();
	}
	RefreshAI(false);
}

void AODEnemyCharacter::HandleOutOfHealth(AActor* DamageInstigator)
{
	Super::HandleOutOfHealth(DamageInstigator);
	CombatTarget.Reset();
	RefreshAI(false);
}

void AODEnemyCharacter::RefreshAI(bool bRestart) const
{
	if (AODAIController* AI = Cast<AODAIController>(GetController()))
	{
		AI->RefreshLogic(bRestart);
	}
}

AActor* AODEnemyCharacter::GetLockOnTarget() const
{
	return GetCombatTarget();
}

AActor* AODEnemyCharacter::GetCombatTarget() const
{
	AODCharacterBase* Target = CombatTarget.Get();
	return IsValid(Target) && !Target->IsDead() ? Target : nullptr;
}

AActor* AODEnemyCharacter::UpdateCombatTarget()
{
	if (!bCombatActive || IsDead())
	{
		CombatTarget.Reset();
		return nullptr;
	}

	const float DistanceFromSpawn = FVector::Dist2D(GetActorLocation(), GetSpawnLocation());
	if (bReturningHome && DistanceFromSpawn <= 200.0f)
	{
		bReturningHome = false;
	}

	// 너무 멀리 끌려왔으면 대상을 놓고 돌아간다.
	if (DistanceFromSpawn > LeashRange)
	{
		CombatTarget.Reset();
		bReturningHome = true;
		return nullptr;
	}

	if (AActor* Current = GetCombatTarget())
	{
		return Current;
	}
	CombatTarget.Reset();
	if (bReturningHome)
	{
		return nullptr;
	}

	// 싱글플레이: 대상 후보는 플레이어 하나
	AODCharacterBase* PlayerCharacter = Cast<AODCharacterBase>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (IsValid(PlayerCharacter) && !PlayerCharacter->IsDead() && IsHostileTo(PlayerCharacter)
		&& FVector::Dist2D(GetActorLocation(), PlayerCharacter->GetActorLocation()) <= AggroRange)
	{
		CombatTarget = PlayerCharacter;
	}
	return GetCombatTarget();
}

void AODEnemyCharacter::RefreshPhase()
{
	UODAbilitySystemComponent* ASC = GetODAbilitySystemComponent();
	if (!IsValid(ASC) || Phases.IsEmpty())
	{
		return;
	}

	const float MaxHealth = FMath::Max(ASC->GetNumericAttribute(UODAttributeSet_Health::GetMaxHealthAttribute()), 1.0f);
	const float Ratio = ASC->GetNumericAttribute(UODAttributeSet_Health::GetHealthAttribute()) / MaxHealth;

	// 체력 비율 조건을 만족하는 마지막(가장 깊은) 페이즈
	int32 PhaseIndex = 0;
	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		if (Ratio <= Phases[Index].HealthRatio)
		{
			PhaseIndex = Index;
		}
	}
	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		if (Phases[Index].PhaseTag.IsValid())
		{
			const int32 NewCount = Index == PhaseIndex ? 1 : 0;
			if (ASC->GetTagCount(Phases[Index].PhaseTag) != NewCount)
			{
				ASC->SetLooseGameplayTagCount(Phases[Index].PhaseTag, NewCount);
				if (NewCount > 0)
				{
					UE_LOG(LogODAI, Log, TEXT("%s 페이즈 전환: %s (체력 %.0f%%)"), *GetNameSafe(this), *Phases[Index].PhaseTag.ToString(), Ratio * 100.0f);
				}
			}
		}
	}
}

void AODEnemyCharacter::Revive()
{
	Super::Revive();
	CombatTarget.Reset();
	bReturningHome = false;
	SetCombatActive(!bStartDormant);
	RefreshPhase();
	// 사망으로 멈췄던 AI를 처음 상태부터 다시 돌린다.
	RefreshAI(true);
}

void AODEnemyCharacter::ResetForTest()
{
	ResetToSpawn();
}
