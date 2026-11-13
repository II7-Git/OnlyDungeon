// OnlyDungeon 전투 수치 AttributeSet

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/ODAttributeSetBase.h"
#include "ODAttributeSet_Combat.generated.h"

/**
 * 스태미나, 공격력, 방어력, 무게. 플레이어와 이 수치가 필요한 적이 가진다.
 * 장비 착용 시 GameplayEffect로 공격력·방어력·무게가 바뀐다.
 */
UCLASS()
class ONLYDUNGEON_API UODAttributeSet_Combat : public UODAttributeSetBase
{
	GENERATED_BODY()

public:
	UODAttributeSet_Combat();

	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Combat, Stamina);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Combat, MaxStamina);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Combat, StaminaRegenRate);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Combat, AttackPower);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Combat, Defense);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Combat, Weight);

protected:
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	/** 현재 스태미나. 공격, 회피, 방어에 소모된다. */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Stamina;

	/** 최대 스태미나 */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxStamina;

	/** 초당 스태미나 회복량 (회복 패시브 어빌리티가 참조) */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData StaminaRegenRate;

	/** 공격력 (주로 무기가 결정) */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData AttackPower;

	/** 방어력 (주로 방어구가 결정) */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Defense;

	/** 장비 총 무게. 무거울수록 회피가 둔해진다. */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Combat", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Weight;
};
