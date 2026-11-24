// OnlyDungeon 불꽃 투사체

#include "ODFireProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraShakeBase.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GenericTeamAgentInterface.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

namespace ODFireProjectile
{
	/** 충돌 구체 반경 (불덩이 크기) */
	constexpr float CollisionRadius = 30.0f;

	/** 비행 시간 하한 (0으로 나누기 방지) */
	constexpr float MinFlightTime = 0.1f;

	/** 폭발 한 번에 맞을 수 있는 캐릭터 수 예상치 (싱글플레이라 작다) */
	constexpr int32 ExpectedVictims = 4;
}

AODFireProjectile::AODFireProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(ODFireProjectile::CollisionRadius);
	// 지형·캐릭터에 막히면 터진다. 쏜 캐릭터는 발사 때 무시 목록에 넣는다.
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	BodyEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BodyEffect"));
	BodyEffect->SetupAttachment(Collision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed = 0.0f;
	Movement->MaxSpeed = 0.0f;
	Movement->ProjectileGravityScale = 1.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bAutoActivate = false;
}

void AODFireProjectile::Launch(const FGameplayEffectSpecHandle& InDamageSpec, const FVector& TargetLocation, float FlightTime, float InRadius)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DamageSpec = InDamageSpec;
	Radius = InRadius;

	if (AActor* Shooter = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
	}

	// 정확히 FlightTime 뒤 TargetLocation에 닿는 초기 속도: 위치 = 시작 + v*t + ½g*t²
	const float Time = FMath::Max(FlightTime, ODFireProjectile::MinFlightTime);
	const FVector Gravity(0.0, 0.0, World->GetGravityZ() * Movement->ProjectileGravityScale);
	const FVector Velocity = (TargetLocation - GetActorLocation() - 0.5 * Gravity * Time * Time) / Time;
	Movement->Velocity = Velocity;
	Movement->OnProjectileStop.AddUniqueDynamic(this, &ThisClass::OnProjectileStop);
	Movement->Activate(true);

	if (IsValid(WarningEffect))
	{
		WarningComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, WarningEffect, TargetLocation, FRotator::ZeroRotator, FVector(WarningScale));
	}

	GetWorldTimerManager().SetTimer(LifetimeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		Explode(GetActorLocation());
	}), MaxLifetime, false);

	UE_LOG(LogODCombat, Verbose, TEXT("%s 발사: 착탄 %s, %.2f초"), *GetName(), *TargetLocation.ToCompactString(), Time);
}

void AODFireProjectile::OnProjectileStop(const FHitResult& ImpactResult)
{
	Explode(ImpactResult.bBlockingHit ? FVector(ImpactResult.ImpactPoint) : GetActorLocation());
}

void AODFireProjectile::Explode(const FVector& Location)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	GetWorldTimerManager().ClearTimer(LifetimeTimer);

	if (UNiagaraComponent* Warning = WarningComponent.Get())
	{
		Warning->Deactivate();
	}

	UWorld* World = GetWorld();
	AActor* Shooter = GetInstigator();
	const FGenericTeamId ShooterTeam = FGenericTeamId::GetTeamIdentifier(Shooter);

	// 피해: 반경 안의 다른 팀 캐릭터에게 한 번씩. 쏜 캐릭터의 ASC가 사라졌으면 연출만 한다.
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Shooter);
	if (World && DamageSpec.IsValid() && IsValid(SourceASC))
	{
		// 막기 방향은 터진 지점 기준, 패링은 불가. (스펙은 이 투사체만 쓰므로 직접 고친다)
		DamageSpec.Data->GetContext().AddOrigin(Location);
		DamageSpec.Data->AddDynamicAssetTag(ODGameplayTags::Damage_Unparryable);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(ODFireProjectileExplode), false, this);
		Params.AddIgnoredActor(Shooter);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, Location, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

		TArray<const AActor*, TInlineAllocator<ODFireProjectile::ExpectedVictims>> Damaged;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AODCharacterBase* Victim = Cast<AODCharacterBase>(Overlap.GetActor());
			if (!IsValid(Victim) || Victim->IsDead() || Damaged.Contains(Victim) || Victim->GetGenericTeamId() == ShooterTeam)
			{
				continue;
			}
			Damaged.Add(Victim);
			if (UAbilitySystemComponent* TargetASC = Victim->GetAbilitySystemComponent(); IsValid(TargetASC))
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data, TargetASC);
				UE_LOG(LogODCombat, Log, TEXT("%s 폭발 적중: %s"), *GetName(), *GetNameSafe(Victim));
			}
		}
	}

	// 연출
	if (IsValid(ExplosionEffect))
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, Location, FRotator::ZeroRotator, FVector(ExplosionScale));
		if (IsValid(Effect) && ExplosionEffectDuration > 0.0f && World)
		{
			FTimerHandle StopHandle;
			World->GetTimerManager().SetTimer(StopHandle, FTimerDelegate::CreateWeakLambda(Effect, [Effect]()
			{
				Effect->Deactivate();
			}), ExplosionEffectDuration, false);
		}
	}
	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Location);
	}
	if (ExplosionShake)
	{
		UGameplayStatics::PlayWorldCameraShake(this, ExplosionShake, Location, 0.0f, ExplosionShakeRadius, 1.0f, false);
	}

	Destroy();
}

void AODFireProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LifetimeTimer);
	if (UNiagaraComponent* Warning = WarningComponent.Get())
	{
		Warning->Deactivate();
	}
	Super::EndPlay(EndPlayReason);
}
