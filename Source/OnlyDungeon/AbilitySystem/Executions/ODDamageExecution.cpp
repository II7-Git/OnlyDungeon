// OnlyDungeon 피해 계산

#include "ODDamageExecution.h"

#include "AbilitySystem/Attributes/ODAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "Core/ODGameplayTags.h"

namespace
{
	/** 계산에 쓰는 Attribute 캡처 정의 */
	struct FODDamageStatics
	{
		FGameplayEffectAttributeCaptureDefinition AttackPowerDef;
		FGameplayEffectAttributeCaptureDefinition DefenseDef;

		FODDamageStatics()
		{
			// 공격력은 공격자(Source)에서, 적용 시점 값을 스냅샷으로 가져온다.
			AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(UODAttributeSet_Combat::GetAttackPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
			// 방어력은 피격자(Target)의 현재 값을 가져온다.
			DefenseDef = FGameplayEffectAttributeCaptureDefinition(UODAttributeSet_Combat::GetDefenseAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
		}
	};

	const FODDamageStatics& DamageStatics()
	{
		static FODDamageStatics Statics;
		return Statics;
	}
}

UODDamageExecution::UODDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().DefenseDef);
}

void UODDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	FAggregatorEvaluateParameters EvaluateParams;
	EvaluateParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluateParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 공격자나 피격자에게 전투 AttributeSet이 없으면 0으로 계산한다. (예: 파괴 가능한 오브젝트)
	float AttackPower = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackPowerDef, EvaluateParams, AttackPower);

	float Defense = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DefenseDef, EvaluateParams, Defense);

	const float BaseDamage = Spec.GetSetByCallerMagnitude(ODGameplayTags::Data_Damage, false, 0.0f);
	const float BasePoiseDamage = Spec.GetSetByCallerMagnitude(ODGameplayTags::Data_PoiseDamage, false, 0.0f);

	// [임시 계산식] 최종 피해 = 기본 피해 + 공격력 - 방어력 (최소 0)
	const float FinalDamage = FMath::Max(BaseDamage + AttackPower - Defense, 0.0f);
	const float FinalPoiseDamage = FMath::Max(BasePoiseDamage, 0.0f);

	if (FinalDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UODAttributeSet_Health::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
	}

	if (FinalPoiseDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UODAttributeSet_Health::GetIncomingPoiseDamageAttribute(), EGameplayModOp::Additive, FinalPoiseDamage));
	}
}
