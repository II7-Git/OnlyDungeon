// OnlyDungeon 어빌리티 기반 클래스

#include "ODGameplayAbility.h"

#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Character/ODCharacterBase.h"
#include "Character/ODCombatModeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "Core/ODGameplayTags.h"

FGameplayTagContainer UODGameplayAbility::GetCurrentBlockingTags(const UAbilitySystemComponent& ASC) const
{
	FGameplayTagContainer OwnedTags;
	ASC.GetOwnedGameplayTags(OwnedTags);
	return ActivationBlockedTags.Filter(OwnedTags);
}

UODGameplayAbility::UODGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 싱글플레이 기준: 액터마다 인스턴스 하나를 만들어 상태(콤보 단계 등)를 보관할 수 있게 한다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

AODCharacterBase* UODGameplayAbility::GetODCharacterFromActorInfo() const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info ? Cast<AODCharacterBase>(Info->AvatarActor.Get()) : nullptr;
}

UODAbilitySystemComponent* UODGameplayAbility::GetODAbilitySystemComponentFromActorInfo() const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info ? Cast<UODAbilitySystemComponent>(Info->AbilitySystemComponent.Get()) : nullptr;
}

FVector UODGameplayAbility::GetMovementInputDirection(float DeadZone) const
{
	const AODCharacterBase* Character = GetODCharacterFromActorInfo();
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	// 공격 중처럼 이동 입력이 막힌 동안에도 플레이어가 누르고 있는 방향을 쓴다. (캔슬 회피 방향 등)
	FVector Input = Character->GetDesiredMoveDirection();
	if (Input.SizeSquared2D() < FMath::Square(DeadZone))
	{
		if (const UCharacterMovementComponent* Movement = Character->GetCharacterMovement(); IsValid(Movement))
		{
			Input = Movement->GetCurrentAcceleration();
		}
	}

	Input.Z = 0.0f;
	if (Input.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	return Input.GetSafeNormal2D();
}

FVector UODGameplayAbility::GetAimDirection(float DeadZone) const
{
	const AODCharacterBase* Character = GetODCharacterFromActorInfo();
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	// 대상이 있으면(플레이어 락온, 적 AI의 공격 대상) 대상을 향한다.
	if (const AActor* AimTarget = Character->GetLockOnTarget(); IsValid(AimTarget))
	{
		FVector ToTarget = AimTarget->GetActorLocation() - Character->GetActorLocation();
		ToTarget.Z = 0.0f;
		if (!ToTarget.IsNearlyZero())
		{
			return ToTarget.GetSafeNormal2D();
		}
	}

	// 필드 모드(쿼터뷰): 마우스 커서 방향. 이동 방향과 무관하게 커서 쪽으로 공격한다.
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); IsValid(ASC) && ASC->HasMatchingGameplayTag(ODGameplayTags::Mode_Field))
	{
		FVector CursorDirection;
		if (const UODCombatModeComponent* ModeComponent = Character->FindComponentByClass<UODCombatModeComponent>(); IsValid(ModeComponent) && ModeComponent->GetCursorAimDirection(CursorDirection))
		{
			return CursorDirection;
		}
	}

	return GetMovementInputDirection(DeadZone);
}

void UODGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// 패시브 어빌리티는 부여되자마자 발동한다.
	if (ActivationPolicy == EODAbilityActivationPolicy::OnSpawn && ActorInfo && !Spec.IsActive())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->TryActivateAbility(Spec.Handle);
		}
	}
}

void UODGameplayAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	// 누르고 있는 동안만 유지되는 어빌리티는 입력을 떼면 종료한다.
	if (ActivationPolicy == EODAbilityActivationPolicy::WhileInputActive && IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UODGameplayAbility::ListenForCancelWindow()
{
	UAbilityTask_WaitGameplayEvent* Task = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ODGameplayTags::Event_Montage_CancelWindow, nullptr, false, true);
	Task->EventReceived.AddDynamic(this, &ThisClass::OnCancelWindowEvent);
	Task->ReadyForActivation();
}

void UODGameplayAbility::OnCancelWindowEvent(FGameplayEventData Payload)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return;
	}

	if (Payload.EventMagnitude > 0.0f)
	{
		OpenCancelWindow();
	}
	else
	{
		ClearCancelWindow();
	}
}

void UODGameplayAbility::OpenCancelWindow()
{
	if (bCancelWindowApplied)
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); IsValid(ASC))
	{
		ASC->AddLooseGameplayTag(ODGameplayTags::State_Cancelable);
		bCancelWindowApplied = true;
	}
}

void UODGameplayAbility::ClearCancelWindow()
{
	if (!bCancelWindowApplied)
	{
		return;
	}
	bCancelWindowApplied = false;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); IsValid(ASC))
	{
		ASC->RemoveLooseGameplayTag(ODGameplayTags::State_Cancelable);
	}
}

void UODGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	ClearCancelWindow();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
