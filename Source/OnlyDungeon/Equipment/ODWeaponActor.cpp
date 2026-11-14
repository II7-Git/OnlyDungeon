// OnlyDungeon 무기 액터

#include "ODWeaponActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/StaticMeshComponent.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Equipment/ODWeaponData.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDrawWeaponTrace(
		TEXT("OD.Debug.WeaponTrace"),
		false,
		TEXT("무기 판정 스윕을 화면에 표시한다. (0: 끔, 1: 켬)"));
}

AODWeaponActor::AODWeaponActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCanEverAffectNavigation(false);
	RootComponent = MeshComponent;
}

void AODWeaponActor::InitializeFromData(const UODWeaponData* InWeaponData)
{
	if (!IsValid(InWeaponData))
	{
		return;
	}

	MeshComponent->SetStaticMesh(InWeaponData->WeaponMesh);
	TraceStart = InWeaponData->TraceStart;
	TraceEnd = InWeaponData->TraceEnd;
	TraceRadius = InWeaponData->TraceRadius;
	TraceSamples = FMath::Max(InWeaponData->TraceSamples, 2);
	FieldHitRadius = InWeaponData->FieldHitRadius;
	FieldHitHalfAngle = InWeaponData->FieldHitHalfAngle;
}

void AODWeaponActor::BeginTrace()
{
	bTracing = true;
	HitActors.Reset();
	GetTracePoints(PreviousPoints);
}

void AODWeaponActor::TickTrace()
{
	UWorld* World = GetWorld();
	if (!bTracing || !IsValid(World))
	{
		return;
	}

	TArray<FVector> CurrentPoints;
	GetTracePoints(CurrentPoints);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ODWeaponTrace), false);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());

	const FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(TraceRadius);
	const bool bDrawDebug = CVarDrawWeaponTrace.GetValueOnGameThread();

	TArray<FHitResult> Hits;
	for (int32 Index = 0; Index < CurrentPoints.Num() && PreviousPoints.IsValidIndex(Index); ++Index)
	{
		Hits.Reset();
		World->SweepMultiByObjectType(Hits, PreviousPoints[Index], CurrentPoints[Index], FQuat::Identity, ObjectParams, Shape, QueryParams);

		if (bDrawDebug)
		{
			DrawDebugLine(World, PreviousPoints[Index], CurrentPoints[Index], Hits.IsEmpty() ? FColor::Green : FColor::Red, false, 1.0f, 0, 1.0f);
		}

		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!IsValid(HitActor) || HitActors.Contains(HitActor))
			{
				continue;
			}

			HitActors.Add(HitActor);
			SendHitEvent(Hit);

			if (bDrawDebug)
			{
				DrawDebugSphere(World, Hit.ImpactPoint, TraceRadius, 8, FColor::Red, false, 1.0f);
			}
		}
	}

	PreviousPoints = MoveTemp(CurrentPoints);
}

void AODWeaponActor::EndTrace()
{
	bTracing = false;
	PreviousPoints.Reset();
	HitActors.Reset();
}

void AODWeaponActor::PerformAreaHit()
{
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!IsValid(World) || !IsValid(OwnerActor))
	{
		return;
	}

	const FVector Origin = OwnerActor->GetActorLocation();
	const FVector Forward = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(FieldHitHalfAngle));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ODWeaponAreaHit), false);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(OwnerActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(FieldHitRadius), QueryParams);

	const bool bDrawDebug = CVarDrawWeaponTrace.GetValueOnGameThread();
	if (bDrawDebug)
	{
		const FVector Left = Forward.RotateAngleAxis(-FieldHitHalfAngle, FVector::UpVector);
		const FVector Right = Forward.RotateAngleAxis(FieldHitHalfAngle, FVector::UpVector);
		DrawDebugLine(World, Origin, Origin + Left * FieldHitRadius, FColor::Green, false, 1.0f, 0, 1.5f);
		DrawDebugLine(World, Origin, Origin + Right * FieldHitRadius, FColor::Green, false, 1.0f, 0, 1.5f);
		DrawDebugLine(World, Origin + Left * FieldHitRadius, Origin + Forward * FieldHitRadius, FColor::Green, false, 1.0f, 0, 1.5f);
		DrawDebugLine(World, Origin + Forward * FieldHitRadius, Origin + Right * FieldHitRadius, FColor::Green, false, 1.0f, 0, 1.5f);
	}

	TArray<AActor*, TInlineAllocator<16>> Processed;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!IsValid(HitActor) || Processed.Contains(HitActor))
		{
			continue;
		}
		Processed.Add(HitActor);

		// 정면 부채꼴 안에 있는 대상만. (몸이 겹칠 만큼 가까우면 각도와 무관하게 맞는다)
		FVector ToTarget = HitActor->GetActorLocation() - Origin;
		ToTarget.Z = 0.0;
		const double Distance = ToTarget.Size();
		const FVector Direction = Distance > UE_KINDA_SMALL_NUMBER ? ToTarget / Distance : Forward;
		if (Distance > 60.0 && FVector::DotProduct(Forward, Direction) < MinCos)
		{
			continue;
		}

		FHitResult Hit(HitActor, Overlap.GetComponent(), HitActor->GetActorLocation(), -Direction);
		Hit.TraceStart = Origin;
		Hit.TraceEnd = HitActor->GetActorLocation();
		Hit.Location = Hit.ImpactPoint;
		Hit.bBlockingHit = true;
		SendHitEvent(Hit);

		if (bDrawDebug)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, 20.0f, 8, FColor::Red, false, 1.0f);
		}
	}
}

void AODWeaponActor::GetTracePoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset(TraceSamples);
	const FTransform& MeshTransform = MeshComponent->GetComponentTransform();
	for (int32 Index = 0; Index < TraceSamples; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(TraceSamples - 1);
		OutPoints.Emplace(MeshTransform.TransformPosition(FMath::Lerp(TraceStart, TraceEnd, Alpha)));
	}
}

void AODWeaponActor::SendHitEvent(const FHitResult& Hit) const
{
	AActor* OwnerActor = GetOwner();
	UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
	if (!IsValid(OwnerASC))
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = ODGameplayTags::Event_Melee_Hit;
	Payload.Instigator = OwnerActor;
	Payload.Target = Hit.GetActor();
	Payload.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);
	OwnerASC->HandleGameplayEvent(Payload.EventTag, &Payload);

	UE_LOG(LogODCombat, Verbose, TEXT("%s 무기 적중: %s"), *GetNameSafe(OwnerActor), *GetNameSafe(Hit.GetActor()));
}
