// OnlyDungeon 체력·강인도 AttributeSet

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/ODAttributeSetBase.h"
#include "ODAttributeSet_Health.generated.h"

/**
 * 체력과 강인도. 플레이어, 모든 적, 파괴 가능한 오브젝트가 공통으로 가진다.
 * 피해는 메타 Attribute(IncomingDamage / IncomingPoiseDamage)로 들어와 여기서 실제 수치에 반영된다.
 */
UCLASS()
class ONLYDUNGEON_API UODAttributeSet_Health : public UODAttributeSetBase
{
	GENERATED_BODY()

public:
	UODAttributeSet_Health();

	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Health, Health);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Health, MaxHealth);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Health, Poise);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Health, MaxPoise);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Health, IncomingDamage);
	OD_ATTRIBUTE_ACCESSORS(UODAttributeSet_Health, IncomingPoiseDamage);

	/** 체력이 0이 되었을 때 (한 번만 발생, 체력이 회복되면 다시 발생 가능) */
	mutable FODAttributeEventSignature OnOutOfHealth;

	/** 강인도가 0이 되었을 때 */
	mutable FODAttributeEventSignature OnPoiseBroken;

protected:
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/** 최대값 Attribute 범위 안으로 값을 맞춘다. */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

private:
	/** 현재 체력 */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Health;

	/** 최대 체력 */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	/** 현재 강인도 (0이 되면 경직) */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Poise;

	/** 최대 강인도 */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxPoise;

	/** [메타] 이번에 받을 피해량. 피해 계산(ODDamageExecution)이 채우고, 적용 후 0으로 초기화된다. */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Health", meta = (AllowPrivateAccess = "true", HideFromModifiers))
	FGameplayAttributeData IncomingDamage;

	/** [메타] 이번에 받을 강인도 피해량 */
	UPROPERTY(BlueprintReadOnly, Category = "OD|Health", meta = (AllowPrivateAccess = "true", HideFromModifiers))
	FGameplayAttributeData IncomingPoiseDamage;

	/** 체력 소진 이벤트 중복 방지 */
	bool bOutOfHealth = false;

	/** 강인도 붕괴 이벤트 중복 방지 */
	bool bPoiseBroken = false;
};
