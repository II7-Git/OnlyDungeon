// OnlyDungeon 적 캐릭터 기반

#include "ODEnemyCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "TimerManager.h"
#include "UI/ODEnemyNameplateWidget.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "AI/ODAIController.h"
#include "Character/ODEnemyCombatComponent.h"
#include "GameplayEffect.h"
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

	// 머리 위 이름표: 화면 공간으로 그려 거리와 무관하게 같은 크기로 보이고 카메라를 향한다.
	NameplateComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
	NameplateComponent->SetupAttachment(RootComponent);
	NameplateComponent->SetRelativeLocation(FVector(0.0, 0.0, 130.0));
	NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
	NameplateComponent->SetDrawAtDesiredSize(true);
	NameplateComponent->SetWidgetClass(UODEnemyNameplateWidget::StaticClass());
	NameplateComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NameplateComponent->SetGenerateOverlapEvents(false);
	NameplateComponent->SetCanEverAffectNavigation(false);
	NameplateComponent->SetHiddenInGame(true);

	CombatComponent = CreateDefaultSubobject<UODEnemyCombatComponent>(TEXT("EnemyCombat"));

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	// 길찾기 이동도 가속도로 움직인다. 끄면 이동 중에도 가속도가 0이라 애님 BP(속도 > 0 그리고 가속도 ≠ 0일 때 걷기)가
	// 대기 자세로 남아 발이 땅에 붙은 채 미끄러진다.
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;
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

	// 머리 위 이름표
	if (IsValid(NameplateComponent))
	{
		if (bShowNameplate)
		{
			// 캡슐 꼭대기 위에 둔다. (루트 스케일이 적용되므로 스케일 전 높이 기준)
			const float HalfHeight = IsValid(GetCapsuleComponent()) ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 90.0f;
			NameplateComponent->SetRelativeLocation(FVector(0.0, 0.0, HalfHeight + NameplateHeightOffset));
			NameplateComponent->InitWidget();
			if (UODEnemyNameplateWidget* Nameplate = Cast<UODEnemyNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
			{
				Nameplate->SetCharacter(this);
			}
			GetWorldTimerManager().SetTimer(NameplateTimer, this, &ThisClass::RefreshNameplate, 0.2f, true, 0.0f);
		}
		else
		{
			NameplateComponent->SetHiddenInGame(true);
			NameplateComponent->SetVisibility(false);
		}
	}

	// 빙의가 BeginPlay보다 먼저 일어나므로, 상태가 준비된 지금 AI를 시작(대기 상태면 정지)한다.
	RefreshAI(true);
}

void AODEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(NameplateTimer);
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
	if (IsValid(CombatComponent))
	{
		CombatComponent->ResetCombatState();
	}
	RefreshAI(false);
	RefreshNameplate();
}

void AODEnemyCharacter::RefreshNameplate()
{
	if (!IsValid(NameplateComponent))
	{
		return;
	}

	bool bVisible = false;
	if (bShowNameplate && !IsDead())
	{
		const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		const UODAbilitySystemComponent* ASC = GetODAbilitySystemComponent();

		// 가까이 있거나, 플레이어와 싸우는 중이거나, 피해를 입은 상태면 보인다.
		const bool bNear = IsValid(PlayerPawn) && FVector::DistSquared(GetActorLocation(), PlayerPawn->GetActorLocation()) <= FMath::Square(NameplateShowDistance);
		const bool bFightingPlayer = IsValid(PlayerPawn) && GetCombatTarget() == PlayerPawn;
		const bool bDamaged = IsValid(ASC)
			&& ASC->GetNumericAttribute(UODAttributeSet_Health::GetHealthAttribute()) < ASC->GetNumericAttribute(UODAttributeSet_Health::GetMaxHealthAttribute());
		// 매복 중(아직 싸우지 않음)에는 가까이 와도 숨긴다. 이름표가 매복을 들키게 하면 안 된다.
		const bool bHiddenAmbush = IdleBehavior == EODEnemyIdleBehavior::Ambush && !bFightingPlayer && !bDamaged;
		bVisible = !bHiddenAmbush && (bNear || bFightingPlayer || bDamaged);
	}

	if (NameplateComponent->bHiddenInGame == bVisible)
	{
		NameplateComponent->SetHiddenInGame(!bVisible);
	}
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

	// 구역 이탈: 자신이 너무 멀리 끌려왔거나, 싸우던 플레이어가 구역 밖으로 나갔으면 어그로를 풀고 돌아간다.
	const AActor* CurrentTarget = GetCombatTarget();
	const bool bTargetLeftArea = IsValid(CurrentTarget) && FVector::Dist2D(CurrentTarget->GetActorLocation(), GetSpawnLocation()) > LeashRange;
	if (DistanceFromSpawn > LeashRange || bTargetLeftArea)
	{
		if (!bReturningHome)
		{
			Disengage();
		}
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
		&& FVector::Dist2D(GetActorLocation(), PlayerCharacter->GetActorLocation()) <= AggroRange
		&& FVector::Dist2D(PlayerCharacter->GetActorLocation(), GetSpawnLocation()) <= LeashRange)
	{
		CombatTarget = PlayerCharacter;
		// 무리 어그로: 주변 동료도 함께 달려온다.
		if (AllyAlertRadius > 0.0f)
		{
			if (UODFieldSubsystem* Field = GetWorld() ? GetWorld()->GetSubsystem<UODFieldSubsystem>() : nullptr)
			{
				Field->AlertNearbyEnemies(this, PlayerCharacter, AllyAlertRadius);
			}
		}
	}
	return GetCombatTarget();
}

bool AODEnemyCharacter::ReceiveAlert(AODCharacterBase* Target)
{
	if (!bCombatActive || IsDead() || bReturningHome || IsValid(GetCombatTarget()) || !IsValid(Target) || Target->IsDead() || !IsHostileTo(Target))
	{
		return false;
	}
	// 대상이 이미 구역 밖이면 따라가지 않는다.
	if (FVector::Dist2D(Target->GetActorLocation(), GetSpawnLocation()) > LeashRange)
	{
		return false;
	}
	CombatTarget = Target;
	return true;
}

void AODEnemyCharacter::Disengage()
{
	CombatTarget.Reset();
	bReturningHome = true;
	if (IsValid(CombatComponent))
	{
		CombatComponent->ResetCombatState();
	}

	// 소울라이크 규칙: 어그로가 풀리면 체력을 가득 회복한다. (Attribute는 GE로만 변경)
	if (UODAbilitySystemComponent* ASC = GetODAbilitySystemComponent(); IsValid(ASC) && LeashResetEffect)
	{
		ASC->ApplyGameplayEffectToSelf(LeashResetEffect.GetDefaultObject(), 1.0f, ASC->MakeEffectContext());
	}
	UE_LOG(LogODAI, Log, TEXT("%s 구역 이탈 → 어그로 해제, 귀환"), *GetNameSafe(this));
}

FVector AODEnemyCharacter::GetPatrolPoint(int32 Index) const
{
	return PatrolPoints.IsValidIndex(Index) ? SpawnTransform.TransformPosition(PatrolPoints[Index]) : GetSpawnLocation();
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
	if (IsValid(CombatComponent))
	{
		CombatComponent->ResetCombatState();
	}
	bReturningHome = false;
	SetCombatActive(!bStartDormant);
	RefreshPhase();
	RefreshNameplate();
	// 사망으로 멈췄던 AI를 처음 상태부터 다시 돌린다.
	RefreshAI(true);
}

void AODEnemyCharacter::ResetForTest()
{
	ResetToSpawn();
}
