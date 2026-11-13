// OnlyDungeon 입력 설정 (입력 액션 ↔ InputTag 매핑)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ODInputConfig.generated.h"

class UInputAction;

/** 입력 액션 하나와 그에 대응하는 InputTag */
USTRUCT(BlueprintType)
struct FODInputAction
{
	GENERATED_BODY()

	/** Enhanced Input 액션 (IA_*) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	/** 이 액션이 보내는 입력 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/**
 * 입력 액션과 InputTag를 연결하는 데이터 에셋.
 * 어빌리티는 키가 아니라 InputTag로 발동되므로, 키 변경(커스텀 키)은 입력 매핑(IMC)만 바꾸면 된다.
 * 에셋 접두사: DA_ (예: DA_InputConfig_Player)
 */
UCLASS(BlueprintType, Const)
class ONLYDUNGEON_API UODInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 이동/시점처럼 C++ 함수에 직접 연결하는 액션을 태그로 찾는다. */
	UFUNCTION(BlueprintCallable, Category = "OD|Input")
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** 어빌리티 발동용 액션을 태그로 찾는다. */
	UFUNCTION(BlueprintCallable, Category = "OD|Input")
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	/** C++ 함수에 직접 연결하는 액션 (이동, 시점) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = "InputAction"))
	TArray<FODInputAction> NativeInputActions;

	/** 어빌리티를 발동하는 액션 (공격, 회피, 방어, 스킬 등) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = "InputAction"))
	TArray<FODInputAction> AbilityInputActions;
};
