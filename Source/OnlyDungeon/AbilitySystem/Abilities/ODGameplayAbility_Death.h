// OnlyDungeon 사망 어빌리티

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_Death.generated.h"

class UAnimMontage;

/**
 * 체력이 0이 되었을 때(Event.Death) 발동해 사망 몽타주를 재생한다.
 * - 몽타주는 자동 블렌드 아웃을 끈 것을 써서 마지막 자세(쓰러진 모습)를 유지한다.
 * - 부활(AODCharacterBase::Revive)할 때 모든 어빌리티가 취소되며 이 어빌리티도 끝나고 몽타주가 멈춘다.
 *   그래서 몽타주가 끝나도 어빌리티를 끝내지 않는다. (끝내면 몽타주가 멈춰 일어선 자세로 돌아간다)
 * - 사망 판정·태그(State.Dead)·이동 정지·충돌 해제는 캐릭터가 처리한다. 이 어빌리티는 연출만 맡는다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_Death : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_Death(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 사망 몽타주 (자동 블렌드 아웃을 끈 것) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Death", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

private:
	UFUNCTION()
	void OnMontageInterrupted();
};
