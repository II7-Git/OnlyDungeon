// OnlyDungeon 피격 경직 어빌리티

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_HitReact.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * 강인도가 무너졌을 때(Event.PoiseBroken) 발동하는 경직 어빌리티.
 * - 공격자 방향(앞·뒤·좌·우)에 맞는 경직 몽타주를 재생한다. 재생 중인 공격 몽타주는 끊긴다.
 * - 끝나면 강인도 회복 GE를 적용해 다시 버틸 수 있게 한다.
 * - 강인도가 남아 있는 동안의 피격은 경직 없이 피해만 받는다. (기획: 강인도 시스템)
 * - 패링당한 경우(이벤트 Magnitude > 0) ParriedMontage로 더 긴 경직을 준다.
 * - 상태 태그(State.Stunned)는 BP(GA_HitReact)에서 지정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_HitReact : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_HitReact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 앞에서 맞았을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HitReact")
	TObjectPtr<UAnimMontage> FrontMontage;

	/** 뒤에서 맞았을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HitReact")
	TObjectPtr<UAnimMontage> BackMontage;

	/** 왼쪽에서 맞았을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HitReact")
	TObjectPtr<UAnimMontage> LeftMontage;

	/** 오른쪽에서 맞았을 때 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HitReact")
	TObjectPtr<UAnimMontage> RightMontage;

	/** 패링당했을 때 재생할 긴 경직 몽타주 (이벤트 Magnitude > 0이면 사용. 비우면 방향별 몽타주) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HitReact")
	TObjectPtr<UAnimMontage> ParriedMontage;

	/** 경직이 끝날 때 적용할 강인도 회복 GE */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HitReact")
	TSubclassOf<UGameplayEffect> PoiseRecoverEffectClass;

private:
	/** 공격자 위치로 재생할 몽타주 선택 */
	UAnimMontage* SelectMontage(const AActor* Avatar, const AActor* Attacker) const;

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageCancelled();
};
