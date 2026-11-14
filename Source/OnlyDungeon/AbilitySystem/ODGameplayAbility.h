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

	/** 지금 발동을 막고 있는 태그 (ActivationBlockedTags 중 보유 중인 것). 발동 실패 원인 표시용 */
	FGameplayTagContainer GetCurrentBlockingTags(const UAbilitySystemComponent& ASC) const;

	/** 어빌리티를 소유한 OnlyDungeon 캐릭터 (없으면 nullptr) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Ability")
	AODCharacterBase* GetODCharacterFromActorInfo() const;

	/**
	 * 실행 중에 같은 입력이 다시 들어오면 직접 처리하는지 (예: 콤보).
	 * false면 캔슬 구간에서 같은 입력이 들어올 때 ASC가 끊고 새로 발동한다.
	 */
	virtual bool HandlesInputWhileActive() const { return false; }

	/** 진행 중인 콤보의 파생 공격으로 발동될 수 있는지 (예: 약공 → 강공) */
	virtual bool CanActivateAsComboBranch() const { return false; }

	/** 어빌리티를 소유한 OnlyDungeon ASC (없으면 nullptr) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Ability")
	UODAbilitySystemComponent* GetODAbilitySystemComponentFromActorInfo() const;

protected:
	/**
	 * 현재 이동 입력 방향 (월드 기준, Z 제거·정규화). 입력이 없으면 ZeroVector.
	 * 직전 프레임에 소비된 입력을 쓰고, 없으면 이동 컴포넌트의 가속 방향으로 보완한다.
	 */
	FVector GetMovementInputDirection(float DeadZone = 0.1f) const;

	/**
	 * 몽타주의 캔슬 구간 노티파이(Event.Montage.CancelWindow)를 듣기 시작한다.
	 * 구간 동안 State.Cancelable이 붙어, 회피·다른 행동·이동 입력이 이 어빌리티를 끊을 수 있다. (ASC·플레이어가 처리)
	 * 행동 어빌리티의 ActivateAbility에서 몽타주 재생 전에 호출한다.
	 */
	void ListenForCancelWindow();

	/** 캔슬 구간 태그를 즉시 제거 (구간을 벗어나는 섹션 이동 시 등) */
	void ClearCancelWindow();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** 이 어빌리티가 언제 발동되는지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Ability Activation")
	EODAbilityActivationPolicy ActivationPolicy = EODAbilityActivationPolicy::OnInputTriggered;

private:
	UFUNCTION()
	void OnCancelWindowEvent(FGameplayEventData Payload);

	bool bCancelWindowApplied = false;
};
