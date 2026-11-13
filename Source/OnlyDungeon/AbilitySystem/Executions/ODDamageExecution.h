// OnlyDungeon 피해 계산

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ODDamageExecution.generated.h"

/**
 * 피해 GE에서 사용하는 계산 클래스.
 * 입력: SetByCaller Data.Damage / Data.PoiseDamage, 공격자 AttackPower, 피격자 Defense
 * 출력: 피격자의 IncomingDamage / IncomingPoiseDamage (실제 체력 반영은 ODAttributeSet_Health가 처리)
 *
 * ※ 현재 계산식은 임시값이다. 최종 계산식은 별도 승인 후 확정한다.
 */
UCLASS()
class ONLYDUNGEON_API UODDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UODDamageExecution();

protected:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
