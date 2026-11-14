// OnlyDungeon 어빌리티 시스템 컴포넌트

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ODAbilitySystemComponent.generated.h"

/** 입력으로 어빌리티 발동을 시도했지만 실패했을 때 (어빌리티, 사람이 읽을 원인) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FODAbilityActivationFailedDelegate, const UGameplayAbility* /*Ability*/, const FString& /*Reason*/);

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

	/** 어빌리티 발동 실패를 알린다. (HUD 표시용) 콤보 다음 타격처럼 ASC 밖에서 실패한 경우에도 호출한다. */
	void ReportActivationFailure(const UGameplayAbility* Ability, const FString& Reason);

	/** 입력으로 발동하려던 어빌리티가 실패했을 때 */
	FODAbilityActivationFailedDelegate OnAbilityActivationFailed;

	/**
	 * 진행 중인 콤보에서 파생 공격으로 넘어간다. (콤보 어빌리티가 입력 구간에서 호출)
	 * InputTag에 연결된 파생 가능 어빌리티를 찾아, 진행 중인 행동을 끊고 State.ComboBranch를 잠시 붙인 채 발동한다.
	 * @return 발동에 성공하면 true
	 */
	bool TryActivateComboBranch(const FGameplayTag& InputTag);
};
