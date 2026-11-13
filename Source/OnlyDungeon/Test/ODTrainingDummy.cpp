// OnlyDungeon 테스트용 허수아비

#include "ODTrainingDummy.h"

#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Core/ODGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AODTrainingDummy::AODTrainingDummy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AutoAttackInputTag = ODGameplayTags::InputTag_Attack_Light;
}

void AODTrainingDummy::BeginPlay()
{
	Super::BeginPlay();

	if (AutoAttackInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(AutoAttackHandle, this, &ThisClass::TryAutoAttack, AutoAttackInterval, true, AutoAttackInterval);
	}
}

void AODTrainingDummy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AutoAttackHandle);
	Super::EndPlay(EndPlayReason);
}

void AODTrainingDummy::TryAutoAttack()
{
	UODAbilitySystemComponent* ASC = GetODAbilitySystemComponent();
	if (!IsValid(ASC) || IsDead() || ASC->HasMatchingGameplayTag(ODGameplayTags::State_Stunned) || ASC->HasMatchingGameplayTag(ODGameplayTags::State_Attacking))
	{
		return;
	}

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValid(Player))
	{
		return;
	}

	FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (ToPlayer.Size() > AutoAttackRange)
	{
		return;
	}

	SetActorRotation(ToPlayer.Rotation());
	ASC->AbilityInputTagPressed(AutoAttackInputTag);
	ASC->AbilityInputTagReleased(AutoAttackInputTag);
}
