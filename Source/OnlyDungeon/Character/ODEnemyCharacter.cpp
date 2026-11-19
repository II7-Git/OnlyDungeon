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
#include "AbilitySystemBlueprintLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Camera/CameraShakeBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODFieldSubsystem.h"
#include "Core/ODBossEncounterSubsystem.h"
#include "Core/ODLogChannels.h"
#include "Engine/World.h"

namespace
{
	/** 이름표 표시 여부(거리·교전·피해) 재판정 주기. 매 프레임일 필요가 없어 타이머로 돈다. */
	constexpr float NameplateRefreshInterval = 0.2f;

	/** 캡슐이 없을 때 이름표 높이 기준으로 쓰는 반높이 (기본 캐릭터 캡슐 크기) */
	constexpr float FallbackCapsuleHalfHeight = 90.0f;

	/** 사망 카메라 흔들림: 이 반경 안은 최대 세기, 바깥 반경까지 줄어든다. */
	constexpr float DeathShakeInnerRadius = 0.0f;
	constexpr float DeathShakeOuterRadius = 4000.0f;
	constexpr float DeathShakeFalloff = 1.0f;

	/** 귀환 중 시작 위치에서 이 거리 안에 들어오면 귀환을 마친 것으로 본다. */
	constexpr float ReturnHomeArrivalDistance = 200.0f;
}

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
			const float HalfHeight = IsValid(GetCapsuleComponent()) ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : FallbackCapsuleHalfHeight;
			NameplateComponent->SetRelativeLocation(FVector(0.0, 0.0, HalfHeight + NameplateHeightOffset));
			NameplateComponent->InitWidget();
			if (UODEnemyNameplateWidget* Nameplate = Cast<UODEnemyNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
			{
				Nameplate->SetCharacter(this);
			}
			GetWorldTimerManager().SetTimer(NameplateTimer, this, &ThisClass::RefreshNameplate, NameplateRefreshInterval, true, 0.0f);
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

	// 등장 연출 (보스 포효 등)
	if (bNewActive && !IsDead() && CombatStartEvent.IsValid())
	{
		FGameplayEventData Payload;
		Payload.EventTag = CombatStartEvent;
		Payload.Instigator = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, CombatStartEvent, Payload);
	}
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
	ClearPhaseLoopEffect();

	// 쓰러짐 연출
	if (IsValid(DeathEffect))
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DeathEffect, GetActorLocation(), GetActorRotation(), FVector(DeathEffectScale));
		if (IsValid(Effect) && DeathEffectDuration > 0.0f)
		{
			FTimerHandle StopHandle;
			GetWorldTimerManager().SetTimer(StopHandle, FTimerDelegate::CreateWeakLambda(Effect, [Effect]()
			{
				Effect->Deactivate();
			}), DeathEffectDuration, false);
		}
	}
	if (DeathCameraShake)
	{
		UGameplayStatics::PlayWorldCameraShake(this, DeathCameraShake, GetActorLocation(), DeathShakeInnerRadius, DeathShakeOuterRadius, DeathShakeFalloff, false);
	}
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
	if (bReturningHome && DistanceFromSpawn <= ReturnHomeArrivalDistance)
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
		StartEliteEncounter();
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
	StartEliteEncounter();
	return true;
}

void AODEnemyCharacter::StartEliteEncounter()
{
	if (!bEliteEncounter)
	{
		return;
	}
	if (UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr;
		Encounter && (!Encounter->IsEncounterActive() || Encounter->GetCurrentBoss() != this))
	{
		// 입구가 없는 보스전: 시작 시 이미 활성인 이 적을 다시 깨우기만 하고, 패배·이탈로 끝나도 대기 상태로 만들지 않는다.
		Encounter->StartEncounter({ this }, nullptr);
	}
}

void AODEnemyCharacter::EndEliteEncounter()
{
	if (!bEliteEncounter)
	{
		return;
	}
	if (UODBossEncounterSubsystem* Encounter = GetWorld() ? GetWorld()->GetSubsystem<UODBossEncounterSubsystem>() : nullptr;
		Encounter && Encounter->IsEncounterActive() && Encounter->GetCurrentBoss() == this)
	{
		Encounter->EndEncounter(false);
	}
}

void AODEnemyCharacter::Disengage()
{
	CombatTarget.Reset();
	EndEliteEncounter();
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

	// 전투 중 더 깊은 페이즈로 넘어갔을 때만 연출 (초기화·부활로 1페이즈로 돌아갈 때는 조용히)
	if (PhaseIndex != CurrentPhaseIndex)
	{
		const bool bAdvanced = PhaseIndex > CurrentPhaseIndex && bCombatActive && !IsDead();
		CurrentPhaseIndex = PhaseIndex;
		ClearPhaseLoopEffect();
		const FODEnemyPhase& Phase = Phases[PhaseIndex];
		if (bAdvanced && Phase.EnterEvent.IsValid())
		{
			FGameplayEventData Payload;
			Payload.EventTag = Phase.EnterEvent;
			Payload.Instigator = this;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, Phase.EnterEvent, Payload);
		}
		if (IsValid(Phase.LoopEffect) && IsValid(GetMesh()) && !IsDead())
		{
			PhaseLoopComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(Phase.LoopEffect, GetMesh(), Phase.LoopEffectSocket, FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, false, true, ENCPoolMethod::None);
		}
	}
}

void AODEnemyCharacter::ClearPhaseLoopEffect()
{
	if (IsValid(PhaseLoopComponent))
	{
		PhaseLoopComponent->DestroyComponent();
	}
	PhaseLoopComponent = nullptr;
}

void AODEnemyCharacter::Revive()
{
	Super::Revive();
	CombatTarget.Reset();
	ClearPhaseLoopEffect();
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
