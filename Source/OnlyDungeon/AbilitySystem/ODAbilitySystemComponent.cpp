// OnlyDungeon 어빌리티 시스템 컴포넌트

#include "ODAbilitySystemComponent.h"

#include "AbilitySystem/ODGameplayAbility.h"
#include "Core/ODLogChannels.h"

UODAbilitySystemComponent::UODAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 틱 대신 입력 이벤트로 동작한다. (ASC 자체 틱은 GameplayEffect 기간 처리 등에 필요하므로 엔진 기본값 유지)
}

void UODAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	// 순회 중에는 어빌리티 목록이 바뀌면 안 되므로, 발동할 핸들을 모아 두었다가 순회가 끝난 뒤 발동한다.
	TArray<FGameplayAbilitySpecHandle> HandlesToActivate;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			{
				continue;
			}

			Spec.InputPressed = true;

			if (Spec.IsActive())
			{
				// 이미 실행 중인 어빌리티에는 입력 눌림만 전달한다. (콤보 입력 등)
				AbilitySpecInputPressed(Spec);
				continue;
			}

			const UODGameplayAbility* ODAbility = Cast<UODGameplayAbility>(Spec.Ability);
			if (ODAbility && ODAbility->GetActivationPolicy() != EODAbilityActivationPolicy::OnSpawn)
			{
				HandlesToActivate.AddUnique(Spec.Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : HandlesToActivate)
	{
		if (!TryActivateAbility(Handle))
		{
			UE_LOG(LogODAbility, Verbose, TEXT("입력 %s: 어빌리티 발동 조건 불충족"), *InputTag.ToString());
		}
	}
}

void UODAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		Spec.InputPressed = false;
		if (Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
		}
	}
}

void UODAbilitySystemComponent::ClearAbilityInput()
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (!Spec.InputPressed)
		{
			continue;
		}

		Spec.InputPressed = false;
		if (Spec.IsActive())
		{
			AbilitySpecInputReleased(Spec);
		}
	}
}
