// OnlyDungeon 락온 컴포넌트

#include "ODLockOnComponent.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

UODLockOnComponent::UODLockOnComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 락온 중에만 Tick을 켠다. (카메라가 매 프레임 대상을 따라가야 함)
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UODLockOnComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	if (AODCharacterBase* Character = Cast<AODCharacterBase>(Owner))
	{
		Character->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleCharacterDeath);
	}
	if (UODCombatModeComponent* ModeComponent = Owner->FindComponentByClass<UODCombatModeComponent>())
	{
		ModeComponent->OnCombatModeChanged.AddUniqueDynamic(this, &ThisClass::HandleCombatModeChanged);
		ModeComponent->OnCinematicChanged.AddUniqueDynamic(this, &ThisClass::HandleCinematicChanged);
	}
}

void UODLockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLockOn();
	Super::EndPlay(EndPlayReason);
}

void UODLockOnComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
	}
	else
	{
		TryLockOn();
	}
}

bool UODLockOnComponent::TryLockOn()
{
	const AODCharacterBase* Character = Cast<AODCharacterBase>(GetOwner());
	if (!IsValid(Character) || Character->IsDead())
	{
		return false;
	}

	// 락온은 보스 모드(3인칭) 전용이다. 필드는 마우스 커서로 조준한다.
	if (const UODCombatModeComponent* ModeComponent = Character->FindComponentByClass<UODCombatModeComponent>())
	{
		if (ModeComponent->GetCombatMode() != EODCombatMode::Boss || ModeComponent->IsInCinematic())
		{
			return false;
		}
	}

	AActor* NewTarget = FindBestTarget();
	if (!IsValid(NewTarget))
	{
		UE_LOG(LogODCombat, Verbose, TEXT("%s 락온: 잡을 대상 없음"), *GetNameSafe(GetOwner()));
		return false;
	}

	SetTarget(NewTarget);
	return true;
}

void UODLockOnComponent::ClearLockOn()
{
	SetTarget(nullptr);
}

FVector UODLockOnComponent::GetTargetAimLocation() const
{
	const AActor* TargetActor = Target.Get();
	return IsValid(TargetActor) ? TargetActor->GetActorLocation() + FVector(0.0, 0.0, TargetHeightOffset) : FVector::ZeroVector;
}

void UODLockOnComponent::SetTarget(AActor* NewTarget)
{
	AActor* OldTarget = Target.Get();
	if (OldTarget == NewTarget)
	{
		return;
	}

	if (AODCharacterBase* OldCharacter = Cast<AODCharacterBase>(OldTarget))
	{
		OldCharacter->OnDeath.RemoveDynamic(this, &ThisClass::HandleCharacterDeath);
	}

	Target = NewTarget;
	const bool bLockedOn = IsValid(NewTarget);

	if (AODCharacterBase* NewCharacter = Cast<AODCharacterBase>(NewTarget))
	{
		NewCharacter->OnDeath.AddUniqueDynamic(this, &ThisClass::HandleCharacterDeath);
	}

	if (const AODCharacterBase* Character = Cast<AODCharacterBase>(GetOwner()))
	{
		if (UODAbilitySystemComponent* ASC = Character->GetODAbilitySystemComponent(); IsValid(ASC))
		{
			ASC->SetLooseGameplayTagCount(ODGameplayTags::State_LockOn, bLockedOn ? 1 : 0);
		}
	}

	ApplyCharacterFacing(bLockedOn);
	SetComponentTickEnabled(bLockedOn);

	UE_LOG(LogODCombat, Log, TEXT("%s 락온: %s"), *GetNameSafe(GetOwner()), bLockedOn ? *GetNameSafe(NewTarget) : TEXT("해제"));
	OnTargetChanged.Broadcast(NewTarget);
}

void UODLockOnComponent::ApplyCharacterFacing(bool bLockedOn) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (!IsValid(Movement))
	{
		return;
	}

	// 락온 중: 이동 방향이 아니라 컨트롤 방향(= 대상 방향)으로 RotationRate에 맞춰 돈다. (옆걸음·뒷걸음)
	Movement->bOrientRotationToMovement = !bLockedOn;
	Movement->bUseControllerDesiredRotation = bLockedOn;
}

void UODLockOnComponent::GatherCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = IsValid(OwnerPawn) ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	const UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ODLockOnGather), false, OwnerPawn);
	World->OverlapMultiByObjectType(Overlaps, OwnerPawn->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SearchRadius), QueryParams);

	const AODCharacterBase* OwnerCharacter = Cast<AODCharacterBase>(OwnerPawn);
	OutCandidates.Reserve(Overlaps.Num());
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AODCharacterBase* Candidate = Cast<AODCharacterBase>(Overlap.GetActor());
		if (!IsValid(Candidate) || Candidate == OwnerPawn || Candidate->IsDead() || OutCandidates.Contains(Candidate))
		{
			continue;
		}
		if (OwnerCharacter && !OwnerCharacter->IsHostileTo(Candidate))
		{
			continue;
		}

		// 벽 뒤의 대상은 잡지 않는다.
		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(ODLockOnSight), false, OwnerPawn);
		SightParams.AddIgnoredActor(Candidate);
		if (World->LineTraceTestByChannel(ViewLocation, Candidate->GetActorLocation() + FVector(0.0, 0.0, TargetHeightOffset), ECC_Visibility, SightParams))
		{
			continue;
		}
		OutCandidates.Add(Candidate);
	}
}

void UODLockOnComponent::HandleLookInput(const FVector2D& LookAxis)
{
	const UWorld* World = GetWorld();
	if (!IsLockedOn() || !World || FMath::IsNearlyZero(LookAxis.X))
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (LastSwitchTime >= 0.0 && Now - LastSwitchTime < SwitchCooldown)
	{
		FlickAccumulated = 0.0f;
		return;
	}

	// 시간 창이 지났거나 방향이 바뀌면 누적을 새로 시작한다.
	const bool bWindowExpired = FlickWindowStart < 0.0 || Now - FlickWindowStart > SwitchFlickWindow;
	const bool bDirectionChanged = FlickAccumulated * LookAxis.X < 0.0f;
	if (bWindowExpired || bDirectionChanged)
	{
		FlickAccumulated = 0.0f;
		FlickWindowStart = Now;
	}

	FlickAccumulated += LookAxis.X;
	if (FMath::Abs(FlickAccumulated) >= SwitchFlickThreshold)
	{
		const float Direction = FMath::Sign(FlickAccumulated);
		FlickAccumulated = 0.0f;
		FlickWindowStart = -1.0;
		if (SwitchTarget(Direction))
		{
			LastSwitchTime = Now;
		}
	}
}

bool UODLockOnComponent::SwitchTarget(float Direction)
{
	const AActor* OwnerActor = GetOwner();
	const AActor* Current = Target.Get();
	if (!IsValid(OwnerActor) || !IsValid(Current) || FMath::IsNearlyZero(Direction))
	{
		return false;
	}

	// 플레이어에서 현재 대상을 보는 방향을 기준으로, 원하는 쪽(좌/우)에서 가장 가까운 각도의 적을 고른다.
	const FVector Origin = OwnerActor->GetActorLocation();
	const FVector BaseDirection = (Current->GetActorLocation() - Origin).GetSafeNormal2D();
	if (BaseDirection.IsNearlyZero())
	{
		return false;
	}

	TArray<AActor*> Candidates;
	GatherCandidates(Candidates);

	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AActor* Candidate : Candidates)
	{
		if (Candidate == Current)
		{
			continue;
		}
		const FVector ToCandidate = (Candidate->GetActorLocation() - Origin).GetSafeNormal2D();
		// 양수 = 기준 방향의 오른쪽
		const float SignedAngle = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(BaseDirection, ToCandidate).Z, FVector::DotProduct(BaseDirection, ToCandidate)));
		if (SignedAngle * Direction <= 0.0f || FMath::Abs(SignedAngle) > MaxSwitchAngle)
		{
			continue;
		}
		const float Score = FMath::Abs(SignedAngle) + FVector::Dist2D(Origin, Candidate->GetActorLocation()) * 0.005f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	if (!IsValid(Best))
	{
		UE_LOG(LogODCombat, Verbose, TEXT("%s 락온 전환: %s 쪽에 대상 없음"), *GetNameSafe(GetOwner()), Direction > 0.0f ? TEXT("오른") : TEXT("왼"));
		return false;
	}

	SetTarget(Best);
	return true;
}

AActor* UODLockOnComponent::FindBestTarget() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = IsValid(OwnerPawn) ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	const UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ODLockOnSearch), false, OwnerPawn);
	World->OverlapMultiByObjectType(Overlaps, OwnerPawn->GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SearchRadius), QueryParams);

	const float MinCos = FMath::Cos(FMath::DegreesToRadians(SearchHalfAngle));
	AActor* BestTarget = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	TSet<const AActor*> Checked;
	Checked.Reserve(Overlaps.Num());

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AODCharacterBase* Candidate = Cast<AODCharacterBase>(Overlap.GetActor());
		if (!IsValid(Candidate) || Candidate == OwnerPawn || Candidate->IsDead() || Checked.Contains(Candidate))
		{
			continue;
		}
		Checked.Add(Candidate);
		if (const AODCharacterBase* OwnerCharacter = Cast<AODCharacterBase>(OwnerPawn); OwnerCharacter && !OwnerCharacter->IsHostileTo(Candidate))
		{
			continue;
		}

		const FVector AimLocation = Candidate->GetActorLocation() + FVector(0.0, 0.0, TargetHeightOffset);
		const FVector ToCandidate = AimLocation - ViewLocation;
		const float Distance = ToCandidate.Size();
		if (Distance <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Cos = FVector::DotProduct(ViewForward, ToCandidate / Distance);
		if (Cos < MinCos)
		{
			continue;
		}

		// 벽 뒤의 대상은 잡지 않는다.
		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(ODLockOnSight), false, OwnerPawn);
		SightParams.AddIgnoredActor(Candidate);
		if (World->LineTraceTestByChannel(ViewLocation, AimLocation, ECC_Visibility, SightParams))
		{
			continue;
		}

		// 화면 중앙에 가까울수록 우선하고, 비슷하면 가까운 대상을 고른다.
		const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.0f, 1.0f)));
		const float Score = AngleDegrees + Distance * 0.01f;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

void UODLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const AActor* TargetActor = Target.Get();
	if (!IsValid(OwnerPawn) || !IsValid(TargetActor))
	{
		ClearLockOn();
		return;
	}

	const FVector OwnerLocation = OwnerPawn->GetActorLocation();
	const FVector AimLocation = GetTargetAimLocation();
	if (FVector::DistSquared(OwnerLocation, AimLocation) > FMath::Square(BreakDistance))
	{
		ClearLockOn();
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		return;
	}

	// 캐릭터에서 대상을 보는 방향으로 카메라를 돌린다. (캐릭터도 이 Yaw를 따라 대상을 바라본다)
	FRotator Desired = (AimLocation - OwnerLocation).Rotation();
	Desired.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Desired.Pitch) + CameraPitchOffset, CameraPitchRange.X, CameraPitchRange.Y);
	Desired.Roll = 0.0f;

	const FRotator Current = PC->GetControlRotation();
	PC->SetControlRotation(CameraInterpSpeed > 0.0f ? FMath::RInterpTo(Current, Desired, DeltaTime, CameraInterpSpeed) : Desired);
}

void UODLockOnComponent::HandleCharacterDeath(AODCharacterBase* DeadCharacter)
{
	// 대상이 죽었거나 본인이 죽으면 푼다.
	if (DeadCharacter == Target.Get() || DeadCharacter == GetOwner())
	{
		ClearLockOn();
	}
}

void UODLockOnComponent::HandleCombatModeChanged(EODCombatMode NewMode)
{
	if (NewMode != EODCombatMode::Boss)
	{
		ClearLockOn();
	}
}

void UODLockOnComponent::HandleCinematicChanged(bool bInCinematic)
{
	if (bInCinematic)
	{
		ClearLockOn();
	}
}
