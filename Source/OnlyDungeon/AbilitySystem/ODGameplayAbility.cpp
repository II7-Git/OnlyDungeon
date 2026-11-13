// OnlyDungeon 어빌리티 기반 클래스

#include "ODGameplayAbility.h"

#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Character/ODCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	FVector Input = Character->GetLastMovementInputVector();
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
