// OnlyDungeon 어빌리티 세트 (부여할 어빌리티·이펙트 묶음)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayAbilitySpec.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "ODAbilitySet.generated.h"

class UODGameplayAbility;
class UGameplayEffect;
class UODAbilitySystemComponent;

/** 세트에 포함된 어빌리티 한 개 */
USTRUCT(BlueprintType)
struct FODAbilitySet_GameplayAbility
{
	GENERATED_BODY()

	/** 부여할 어빌리티 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UODGameplayAbility> Ability;

	/** 어빌리티 레벨 */
	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	/** 이 어빌리티를 발동하는 입력 태그 (패시브면 비워 둠) */
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/** 세트에 포함된 GameplayEffect 한 개 (초기 수치 설정 등) */
USTRUCT(BlueprintType)
struct FODAbilitySet_GameplayEffect
{
	GENERATED_BODY()

	/** 적용할 이펙트 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect;

	/** 이펙트 레벨 */
	UPROPERTY(EditDefaultsOnly)
	float EffectLevel = 1.0f;
};

/** 세트로 부여한 결과 핸들. 장비 해제처럼 세트를 회수할 때 사용한다. */
USTRUCT(BlueprintType)
struct FODAbilitySet_GrantedHandles
{
	GENERATED_BODY()

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);

	/** 부여했던 어빌리티와 이펙트를 모두 제거한다. */
	void TakeFromAbilitySystem(UODAbilitySystemComponent* ASC);

protected:
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;
};

/**
 * 캐릭터 기본 능력, 장비가 주는 스킬 등 "함께 부여·회수되는 묶음"을 정의하는 데이터 에셋.
 * 에셋 접두사: DA_ (예: DA_AbilitySet_Player)
 */
UCLASS(BlueprintType, Const)
class ONLYDUNGEON_API UODAbilitySet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 세트를 ASC에 부여한다.
	 * @param OutGrantedHandles 나중에 회수하려면 전달한다. (nullptr이면 회수 불가)
	 * @param SourceObject      부여 출처 (예: 장비 인스턴스)
	 */
	void GiveToAbilitySystem(UODAbilitySystemComponent* ASC, FODAbilitySet_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

protected:
	/** 부여할 어빌리티 목록 */
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta = (TitleProperty = "Ability"))
	TArray<FODAbilitySet_GameplayAbility> GrantedGameplayAbilities;

	/** 적용할 이펙트 목록 (초기 수치 GE 등) */
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects", meta = (TitleProperty = "GameplayEffect"))
	TArray<FODAbilitySet_GameplayEffect> GrantedGameplayEffects;
};
