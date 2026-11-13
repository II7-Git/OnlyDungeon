// OnlyDungeon 전투 수치 AttributeSet

#include "ODAttributeSet_Combat.h"

UODAttributeSet_Combat::UODAttributeSet_Combat()
{
	// 안전한 기본값. 실제 초기값은 어빌리티 세트의 초기화 GE와 장비 GE로 덮어쓴다.
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitStaminaRegenRate(20.0f);
	InitAttackPower(10.0f);
	InitDefense(0.0f);
	InitWeight(0.0f);
}

void UODAttributeSet_Combat::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UODAttributeSet_Combat::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UODAttributeSet_Combat::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// 최대 스태미나가 줄어들면 현재 스태미나도 맞춘다.
	if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
	{
		if (UAbilitySystemComponent* ASC = GetOwningASC())
		{
			ASC->ApplyModToAttribute(GetStaminaAttribute(), EGameplayModOp::Override, NewValue);
		}
	}
}

void UODAttributeSet_Combat::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
	else if (Attribute == GetStaminaRegenRateAttribute() || Attribute == GetAttackPowerAttribute()
		|| Attribute == GetDefenseAttribute() || Attribute == GetWeightAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}
