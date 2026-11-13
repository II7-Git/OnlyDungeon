// OnlyDungeon 체력·강인도 AttributeSet

#include "ODAttributeSet_Health.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Core/ODLogChannels.h"

UODAttributeSet_Health::UODAttributeSet_Health()
{
	// 안전한 기본값. 실제 초기값은 어빌리티 세트의 초기화 GE로 덮어쓴다.
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitPoise(50.0f);
	InitMaxPoise(50.0f);
	InitIncomingDamage(0.0f);
	InitIncomingPoiseDamage(0.0f);
}

void UODAttributeSet_Health::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UODAttributeSet_Health::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UODAttributeSet_Health::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	UAbilitySystemComponent* ASC = GetOwningASC();
	if (!IsValid(ASC))
	{
		return;
	}

	// 최대값이 줄어들면 현재값도 최대값 이하로 맞춘다. (GE를 통해 변경되도록 ASC에 위임)
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		ASC->ApplyModToAttribute(GetHealthAttribute(), EGameplayModOp::Override, NewValue);
	}
	else if (Attribute == GetMaxPoiseAttribute() && GetPoise() > NewValue)
	{
		ASC->ApplyModToAttribute(GetPoiseAttribute(), EGameplayModOp::Override, NewValue);
	}

	// 부활 등으로 다시 차오르면 이벤트를 다시 받을 수 있게 한다.
	if (Attribute == GetHealthAttribute() && NewValue > 0.0f)
	{
		bOutOfHealth = false;
	}
	else if (Attribute == GetPoiseAttribute() && NewValue > 0.0f)
	{
		bPoiseBroken = false;
	}
}

void UODAttributeSet_Health::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	AActor* Instigator = Data.EffectSpec.GetEffectContext().GetOriginalInstigator();

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float DamageDone = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (DamageDone > 0.0f)
		{
			SetHealth(FMath::Clamp(GetHealth() - DamageDone, 0.0f, GetMaxHealth()));
			UE_LOG(LogODCombat, Verbose, TEXT("%s: 피해 %.1f → 체력 %.1f"), *GetNameSafe(GetOwningActor()), DamageDone, GetHealth());
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingPoiseDamageAttribute())
	{
		const float PoiseDamageDone = GetIncomingPoiseDamage();
		SetIncomingPoiseDamage(0.0f);

		if (PoiseDamageDone > 0.0f)
		{
			SetPoise(FMath::Clamp(GetPoise() - PoiseDamageDone, 0.0f, GetMaxPoise()));
		}
	}

	if (GetHealth() <= 0.0f && !bOutOfHealth)
	{
		bOutOfHealth = true;
		OnOutOfHealth.Broadcast(Instigator);
	}

	if (GetPoise() <= 0.0f && !bPoiseBroken)
	{
		bPoiseBroken = true;
		OnPoiseBroken.Broadcast(Instigator);
	}
}

void UODAttributeSet_Health::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetPoiseAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxPoise());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxPoiseAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}
