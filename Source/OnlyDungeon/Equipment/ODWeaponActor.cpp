// OnlyDungeon 무기 액터

#include "ODWeaponActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Equipment/ODWeaponData.h"
#include "Character/ODCharacterBase.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDrawWeaponTrace(
		TEXT("OD.Debug.WeaponTrace"),
		false,
		TEXT("무기 판정 스윕을 화면에 표시한다. (0: 끔, 1: 켬)"));

	/** 스윕 한 번에 겹칠 수 있는 대상 수 예상치 (HitBuffer 초기 용량) */
	constexpr int32 ExpectedHitsPerSweep = 8;

	/** 필드 범위 판정에서 이 거리 안이면 몸이 겹친 것으로 보고 각도와 무관하게 맞힌다. */
	constexpr double AreaHitOverlapDistance = 60.0;

	/** 디버그 표시 유지 시간과 크기 */
	constexpr float DebugDrawDuration = 1.0f;
	constexpr float DebugLineThickness = 1.0f;
	constexpr float DebugAreaLineThickness = 1.5f;
	constexpr float DebugAreaHitSphereRadius = 20.0f;
	constexpr int32 DebugSphereSegments = 8;
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
	TraceDownReach = InWeaponData->TraceDownReach;
	bTraceOffhand = InWeaponData->bTraceOffhand;
	OffhandSocket = InWeaponData->OffhandAttachSocket;
	OffhandTraceStart = InWeaponData->OffhandTraceStart;
	OffhandTraceEnd = InWeaponData->OffhandTraceEnd;
	FieldHitRadius = InWeaponData->FieldHitRadius;
	FieldHitHalfAngle = InWeaponData->FieldHitHalfAngle;

	// 판정 버퍼를 무기 설정에 맞춰 미리 잡아 두어 휘두르는 동안 재할당이 없게 한다.
	const int32 PointCount = bTraceOffhand ? TraceSamples * 2 : TraceSamples;
	PreviousPoints.Reserve(PointCount);
	CurrentPoints.Reserve(PointCount);
	HitBuffer.Reserve(ExpectedHitsPerSweep);
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

	GetTracePoints(CurrentPoints);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ODWeaponTrace), false);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());

	const FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
	// 아래로 늘린 판정은 세운 캡슐로 쓸어 내린다.
	const FCollisionShape Shape = TraceDownReach > 0.0f ? FCollisionShape::MakeCapsule(TraceRadius, TraceRadius + TraceDownReach * 0.5f) : FCollisionShape::MakeSphere(TraceRadius);
	const FVector ShapeOffset(0.0, 0.0, -TraceDownReach * 0.5f);
	const bool bDrawDebug = CVarDrawWeaponTrace.GetValueOnGameThread();

	for (int32 Index = 0; Index < CurrentPoints.Num() && PreviousPoints.IsValidIndex(Index); ++Index)
	{
		HitBuffer.Reset();
		World->SweepMultiByObjectType(HitBuffer, PreviousPoints[Index] + ShapeOffset, CurrentPoints[Index] + ShapeOffset, FQuat::Identity, ObjectParams, Shape, QueryParams);

		if (bDrawDebug)
		{
			DrawDebugLine(World, PreviousPoints[Index], CurrentPoints[Index], HitBuffer.IsEmpty() ? FColor::Green : FColor::Red, false, DebugDrawDuration, 0, DebugLineThickness);
		}

		for (const FHitResult& Hit : HitBuffer)
		{
			AActor* HitActor = Hit.GetActor();
			if (!IsValid(HitActor) || HitActors.Contains(HitActor))
			{
				continue;
			}
			// 같은 진영(적끼리)은 맞지 않는다.
			if (const AODCharacterBase* OwnerCharacter = Cast<AODCharacterBase>(GetOwner()); OwnerCharacter && !OwnerCharacter->IsHostileTo(HitActor))
			{
				continue;
			}

			HitActors.Add(HitActor);
			SendHitEvent(Hit);

			if (bDrawDebug)
			{
				DrawDebugSphere(World, Hit.ImpactPoint, TraceRadius, DebugSphereSegments, FColor::Red, false, DebugDrawDuration);
			}
		}
	}

	// 버퍼를 맞바꿔 다음 프레임의 '이전 위치'로 쓴다. (복사·할당 없음)
	Swap(PreviousPoints, CurrentPoints);
}

void AODWeaponActor::EndTrace()
{
	bTracing = false;
	PreviousPoints.Reset();
	CurrentPoints.Reset();
	HitBuffer.Reset();
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
		DrawDebugLine(World, Origin, Origin + Left * FieldHitRadius, FColor::Green, false, DebugDrawDuration, 0, DebugAreaLineThickness);
		DrawDebugLine(World, Origin, Origin + Right * FieldHitRadius, FColor::Green, false, DebugDrawDuration, 0, DebugAreaLineThickness);
		DrawDebugLine(World, Origin + Left * FieldHitRadius, Origin + Forward * FieldHitRadius, FColor::Green, false, DebugDrawDuration, 0, DebugAreaLineThickness);
		DrawDebugLine(World, Origin + Forward * FieldHitRadius, Origin + Right * FieldHitRadius, FColor::Green, false, DebugDrawDuration, 0, DebugAreaLineThickness);
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
		if (const AODCharacterBase* OwnerCharacter = Cast<AODCharacterBase>(OwnerActor); OwnerCharacter && !OwnerCharacter->IsHostileTo(HitActor))
		{
			continue;
		}

		// 정면 부채꼴 안에 있는 대상만. (몸이 겹칠 만큼 가까우면 각도와 무관하게 맞는다)
		FVector ToTarget = HitActor->GetActorLocation() - Origin;
		ToTarget.Z = 0.0;
		const double Distance = ToTarget.Size();
		const FVector Direction = Distance > UE_KINDA_SMALL_NUMBER ? ToTarget / Distance : Forward;
		if (Distance > AreaHitOverlapDistance && FVector::DotProduct(Forward, Direction) < MinCos)
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
			DrawDebugSphere(World, Hit.ImpactPoint, DebugAreaHitSphereRadius, DebugSphereSegments, FColor::Red, false, DebugDrawDuration);
		}
	}
}

void AODWeaponActor::GetTracePoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset(bTraceOffhand ? TraceSamples * 2 : TraceSamples);
	const FTransform& MeshTransform = MeshComponent->GetComponentTransform();
	for (int32 Index = 0; Index < TraceSamples; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(TraceSamples - 1);
		OutPoints.Emplace(MeshTransform.TransformPosition(FMath::Lerp(TraceStart, TraceEnd, Alpha)));
	}

	// 보조 손: 소유자 메시 소켓 기준 (지점 순서가 매 프레임 같아야 이전 위치와 짝지어 쓸어 낼 수 있다)
	if (bTraceOffhand)
	{
		const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
		const USkeletalMeshComponent* OwnerMesh = IsValid(OwnerCharacter) ? OwnerCharacter->GetMesh() : nullptr;
		if (IsValid(OwnerMesh))
		{
			const FTransform SocketTransform = OwnerMesh->GetSocketTransform(OffhandSocket);
			for (int32 Index = 0; Index < TraceSamples; ++Index)
			{
				const float Alpha = static_cast<float>(Index) / static_cast<float>(TraceSamples - 1);
				OutPoints.Emplace(SocketTransform.TransformPosition(FMath::Lerp(OffhandTraceStart, OffhandTraceEnd, Alpha)));
			}
		}
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
