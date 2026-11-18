// OnlyDungeon 어빌리티 세트

#include "ODAbilitySet.h"

#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "GameplayEffect.h"
#include "Core/ODLogChannels.h"

void FODAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Emplace(Handle);
	}
}

void FODAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Emplace(Handle);
	}
}

void FODAbilitySet_GrantedHandles::TakeFromAbilitySystem(UODAbilitySystemComponent* ASC)
{
	if (!IsValid(ASC))
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& Handle : AbilitySpecHandles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : GameplayEffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	AbilitySpecHandles.Reset();
	GameplayEffectHandles.Reset();
}

void UODAbilitySet::ReapplyInstantEffects(UODAbilitySystemComponent* ASC) const
{
	if (!IsValid(ASC))
	{
		return;
	}
	for (const FODAbilitySet_GameplayEffect& Entry : GrantedGameplayEffects)
	{
		const UGameplayEffect* EffectCDO = IsValid(Entry.GameplayEffect) ? Entry.GameplayEffect->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (EffectCDO && EffectCDO->DurationPolicy == EGameplayEffectDurationType::Instant)
		{
			ASC->ApplyGameplayEffectToSelf(EffectCDO, Entry.EffectLevel, ASC->MakeEffectContext());
		}
	}
}

void UODAbilitySet::GiveToAbilitySystem(UODAbilitySystemComponent* ASC, FODAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject) const
{
	if (!IsValid(ASC))
	{
		UE_LOG(LogODAbility, Warning, TEXT("%s: ASC가 유효하지 않아 어빌리티 세트를 부여하지 못함"), *GetNameSafe(this));
		return;
	}

	// 이펙트를 먼저 적용해 초기 수치를 잡은 뒤 어빌리티를 부여한다. (패시브 어빌리티가 수치를 참조할 수 있음)
	for (int32 Index = 0; Index < GrantedGameplayEffects.Num(); ++Index)
	{
		const FODAbilitySet_GameplayEffect& EffectToGrant = GrantedGameplayEffects[Index];
		if (!IsValid(EffectToGrant.GameplayEffect))
		{
			UE_LOG(LogODAbility, Warning, TEXT("%s: GrantedGameplayEffects[%d]가 비어 있음"), *GetNameSafe(this), Index);
			continue;
		}

		const UGameplayEffect* EffectCDO = EffectToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(EffectCDO, EffectToGrant.EffectLevel, ASC->MakeEffectContext());
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(Handle);
		}
	}

	for (int32 Index = 0; Index < GrantedGameplayAbilities.Num(); ++Index)
	{
		const FODAbilitySet_GameplayAbility& AbilityToGrant = GrantedGameplayAbilities[Index];
		if (!IsValid(AbilityToGrant.Ability))
		{
			UE_LOG(LogODAbility, Warning, TEXT("%s: GrantedGameplayAbilities[%d]가 비어 있음"), *GetNameSafe(this), Index);
			continue;
		}

		UODGameplayAbility* AbilityCDO = AbilityToGrant.Ability->GetDefaultObject<UODGameplayAbility>();

		FGameplayAbilitySpec AbilitySpec(AbilityCDO, AbilityToGrant.AbilityLevel);
		AbilitySpec.SourceObject = SourceObject;
		if (AbilityToGrant.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);
		}

		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(AbilitySpec);
		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(Handle);
		}
	}
}
