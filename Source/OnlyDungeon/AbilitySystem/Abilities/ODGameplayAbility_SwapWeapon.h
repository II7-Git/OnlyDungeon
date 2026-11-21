// OnlyDungeon 무기 교체 어빌리티 (Tab)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_SwapWeapon.generated.h"

class AActor;
class UAnimMontage;
class UODWeaponData;
struct FGameplayEventData;

/**
 * 무기 교체: 게임플레이 이벤트(Event.Weapon.SwapRequest, OptionalObject = 꺼낼 무기 데이터)로 발동한다.
 * 상체 몽타주로 지금 무기를 등에 넣었다가 새 무기를 꺼낸다. (걸으면서 바꿀 수 있다)
 * 몽타주의 무기 교체 구간(Event.Montage.WeaponSwap)이 열리면 = 손이 등에 닿으면 무기를 바꾸고 새 무기를 숨기며,
 * 닫히면 = 등에서 칼을 쥐면 새 무기를 보인다.
 * 교체 중에는 공격·회피 같은 행동(Ability.Action)을 막는다. 경직·사망하면 취소되고, 숨긴 무기는 다시 보이게 한다.
 * 몽타주와 재생 속도는 BP(GA_SwapWeapon)에서 정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_SwapWeapon : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_SwapWeapon(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 넣었다 꺼내는 상체 몽타주 (Event.Montage.WeaponSwap 구간 노티파이 포함) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|SwapWeapon")
	TObjectPtr<UAnimMontage> SwapMontage;

	UPROPERTY(EditDefaultsOnly, Category = "OD|SwapWeapon", meta = (ClampMin = "0.05"))
	float MontagePlayRate = 1.0f;

private:
	UFUNCTION()
	void OnSwapEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	/** 경직·사망 태그가 붙으면 교체를 취소한다. */
	void OnInterruptTagChanged(const FGameplayTag Tag, int32 NewCount);

	void SetWeaponHidden(bool bHidden);

	TWeakObjectPtr<UODWeaponData> PendingWeapon;
	TWeakObjectPtr<AActor> HiddenWeapon;
	bool bSwapped = false;
	FDelegateHandle StunnedHandle;
	FDelegateHandle DeadHandle;
};
