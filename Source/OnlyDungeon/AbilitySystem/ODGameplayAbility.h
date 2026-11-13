// OnlyDungeon 어빌리티 기반 클래스

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ODGameplayAbility.generated.h"

class AODCharacterBase;
class UODAbilitySystemComponent;

/** 어빌리티가 언제 발동되는지 정하는 방식 */
UENUM(BlueprintType)
enum class EODAbilityActivationPolicy : uint8
{
	/** 입력을 누른 순간 1회 발동 (공격, 회피, 스킬) */
	OnInputTriggered,

	/** 입력을 누르고 있는 동안 유지, 떼면 종료 (대검 가드 등) */
	WhileInputActive,

	/** 부여되는 즉시 자동 발동 (스태미나 자연 회복 같은 패시브) */
	OnSpawn
};

/**
 * OnlyDungeon의 모든 어빌리티가 상속하는 기반 클래스.
 * 발동 방식과 캐릭터/ASC 접근 헬퍼를 제공한다.
 * 모드 제한(예: 강공격은 보스전 전용)은 ActivationRequiredTags에 Mode.Boss 등을 지정해 처리한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 이 어빌리티의 발동 방식 */
	EODAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	/** 어빌리티를 소유한 OnlyDungeon 캐릭터 (없으면 nullptr) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Ability")
	AODCharacterBase* GetODCharacterFromActorInfo() const;

	/** 어빌리티를 소유한 OnlyDungeon ASC (없으면 nullptr) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Ability")
	UODAbilitySystemComponent* GetODAbilitySystemComponentFromActorInfo() const;

protected:
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** 이 어빌리티가 언제 발동되는지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Ability Activation")
	EODAbilityActivationPolicy ActivationPolicy = EODAbilityActivationPolicy::OnInputTriggered;
};
