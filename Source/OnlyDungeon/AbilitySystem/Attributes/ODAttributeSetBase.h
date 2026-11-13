// OnlyDungeon AttributeSet 공통 기반

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ODAttributeSetBase.generated.h"

/** Attribute 접근자(Get/Set/Init/GetXXXAttribute)를 한 번에 선언하는 매크로 */
#define OD_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/** Attribute 이벤트 델리게이트 (예: 체력 소진, 강인도 붕괴). 인자: 원인 제공자(없을 수 있음) */
DECLARE_MULTICAST_DELEGATE_OneParam(FODAttributeEventSignature, AActor* /*Instigator*/);

/**
 * OnlyDungeon AttributeSet 공통 기반.
 * 모든 수치 변경은 GameplayEffect를 통해서만 한다. (CLAUDE.md 6장)
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	/** 이 AttributeSet을 소유한 ASC */
	UAbilitySystemComponent* GetOwningASC() const { return GetOwningAbilitySystemComponent(); }
};
