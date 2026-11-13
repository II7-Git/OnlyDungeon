// OnlyDungeon 어빌리티 시스템 컴포넌트

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ODAbilitySystemComponent.generated.h"

/**
 * OnlyDungeon 전용 ASC.
 * 입력 태그(InputTag.*)로 어빌리티를 찾아 발동/해제한다.
 * 어빌리티는 부여될 때 DynamicSpecSourceTags에 InputTag를 받으며, 키는 입력 매핑에서만 결정된다.
 */
UCLASS(ClassGroup = (OnlyDungeon), meta = (BlueprintSpawnableComponent))
class ONLYDUNGEON_API UODAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UODAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 입력 태그에 해당하는 어빌리티를 발동하거나, 이미 발동 중이면 입력 눌림을 전달한다. */
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	/** 입력 태그에 해당하는 어빌리티에 입력 해제를 전달한다. */
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/** 눌려 있는 모든 입력을 해제 처리한다. (모드 전환, 사망 시 사용) */
	void ClearAbilityInput();

	/** 진행 중인 행동 어빌리티(Ability.Action)를 모두 끊는다. 캔슬 구간 입력에서 사용 */
	void CancelActionAbilities();
};
