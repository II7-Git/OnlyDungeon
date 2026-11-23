// OnlyDungeon 피해 계산

#include "ODDamageExecution.h"

#include "AbilitySystem/Attributes/ODAttributeSet_Combat.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "AbilitySystemComponent.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

namespace
{
	/** 계산에 쓰는 Attribute 캡처 정의 */
	struct FODDamageStatics
	{
		FGameplayEffectAttributeCaptureDefinition AttackPowerDef;
		FGameplayEffectAttributeCaptureDefinition DefenseDef;
		FGameplayEffectAttributeCaptureDefinition TargetStaminaDef;

		FODDamageStatics()
		{
			// 공격력은 공격자(Source)에서, 적용 시점 값을 스냅샷으로 가져온다.
			AttackPowerDef = FGameplayEffectAttributeCaptureDefinition(UODAttributeSet_Combat::GetAttackPowerAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
			// 방어력은 피격자(Target)의 현재 값을 가져온다.
			DefenseDef = FGameplayEffectAttributeCaptureDefinition(UODAttributeSet_Combat::GetDefenseAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
			// 가드 시 스태미나로 막을 수 있는지 판단하기 위해 피격자의 현재 스태미나를 가져온다.
			TargetStaminaDef = FGameplayEffectAttributeCaptureDefinition(UODAttributeSet_Combat::GetStaminaAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
		}
	};

	const FODDamageStatics& DamageStatics()
	{
		static FODDamageStatics Statics;
		return Statics;
	}

	// [임시 수치] 방어 관련 값. 무기별 값으로 옮길 때까지 여기서 관리한다.
	/** 정면으로 인정하는 반각(도). 공격자가 이 각도 안에 있어야 패링·가드가 적용된다 */
	constexpr float DefenseFrontHalfAngle = 75.0f;
	/** 가드 시 피해 감소 비율 */
	constexpr float GuardDamageReduction = 0.8f;
	/** 가드 시 막은 피해(감소 전) 1당 소모되는 스태미나 */
	constexpr float GuardStaminaPerDamage = 1.0f;
	/** 가드가 깨질 때 줄 강인도 피해 (강인도를 0으로 만들어 경직시키기 위한 큰 값) */
	constexpr float GuardBreakPoiseDamage = 9999.0f;

	/** 공격자가 피격자의 정면에 있는지 */
	bool IsAttackFromFront(const AActor* Target, const AActor* Attacker)
	{
		if (!IsValid(Target) || !IsValid(Attacker))
		{
			return false;
		}

		FVector ToAttacker = Attacker->GetActorLocation() - Target->GetActorLocation();
		ToAttacker.Z = 0.0f;
		if (!ToAttacker.Normalize())
		{
			return true;
		}

		return FVector::DotProduct(Target->GetActorForwardVector().GetSafeNormal2D(), ToAttacker) >= FMath::Cos(FMath::DegreesToRadians(DefenseFrontHalfAngle));
	}

	/** ASC에 게임플레이 이벤트를 보낸다 */
	void SendEvent(UAbilitySystemComponent* ASC, const FGameplayTag& Tag, AActor* Instigator, AActor* Target, float Magnitude = 0.0f)
	{
		if (!IsValid(ASC))
		{
			return;
		}

		FGameplayEventData Payload;
		Payload.EventTag = Tag;
		Payload.Instigator = Instigator;
		Payload.Target = Target;
		Payload.EventMagnitude = Magnitude;
		ASC->HandleGameplayEvent(Tag, &Payload);
	}
}

UODDamageExecution::UODDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().DefenseDef);
	RelevantAttributesToCapture.Add(DamageStatics().TargetStaminaDef);
}

void UODDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	AActor* TargetActor = IsValid(TargetASC) ? TargetASC->GetAvatarActor() : nullptr;
	AActor* SourceActor = IsValid(SourceASC) ? SourceASC->GetAvatarActor() : nullptr;

	// 1) 무적 상태(회피 무적 구간 등)면 피해·강인도 피해를 모두 무시한다.
	if (IsValid(TargetASC) && TargetASC->HasMatchingGameplayTag(ODGameplayTags::State_Invulnerable))
	{
		UE_LOG(LogODCombat, Verbose, TEXT("피해 무시: 대상 %s 무적 상태"), *GetNameSafe(TargetActor));
		return;
	}

	const bool bFromFront = IsAttackFromFront(TargetActor, SourceActor);

	// 2) 패링 성공: 피해 없음, 공격자는 경직(강인도 붕괴 이벤트), 방어자에게 패링 성공 이벤트
	if (IsValid(TargetASC) && bFromFront && TargetASC->HasMatchingGameplayTag(ODGameplayTags::State_Parrying))
	{
		UE_LOG(LogODCombat, Log, TEXT("패링 성공: %s ← %s"), *GetNameSafe(TargetActor), *GetNameSafe(SourceActor));
		SendEvent(TargetASC, ODGameplayTags::Event_ParrySuccess, SourceActor, TargetActor);
		TargetASC->ExecuteGameplayCue(ODGameplayTags::GameplayCue_Parry, Spec.GetEffectContext());
		// Magnitude 1 = 패링당함 (경직 어빌리티가 긴 경직 몽타주를 고른다)
		SendEvent(SourceASC, ODGameplayTags::Event_PoiseBroken, TargetActor, SourceActor, 1.0f);
		return;
	}

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
	float FinalDamage = FMath::Max(BaseDamage + AttackPower - Defense, 0.0f);
	float FinalPoiseDamage = FMath::Max(BasePoiseDamage, 0.0f);

	// 3) 가드: 정면 공격이면 피해를 줄이고 강인도 피해 없이 스태미나로 막는다. 스태미나가 모자라면 가드가 깨진다.
	if (IsValid(TargetASC) && bFromFront && TargetASC->HasMatchingGameplayTag(ODGameplayTags::State_Defending))
	{
		float TargetStamina = 0.0f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().TargetStaminaDef, EvaluateParams, TargetStamina);

		const float StaminaCost = FinalDamage * GuardStaminaPerDamage;
		FinalDamage *= (1.0f - GuardDamageReduction);

		if (TargetStamina >= StaminaCost)
		{
			FinalPoiseDamage = 0.0f;
			OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UODAttributeSet_Combat::GetStaminaAttribute(), EGameplayModOp::Additive, -StaminaCost));
			SendEvent(TargetASC, ODGameplayTags::Event_GuardBlocked, SourceActor, TargetActor, StaminaCost);
			TargetASC->ExecuteGameplayCue(ODGameplayTags::GameplayCue_GuardBlock, Spec.GetEffectContext());
			UE_LOG(LogODCombat, Log, TEXT("가드: %s (피해 %.1f, 스태미나 -%.1f)"), *GetNameSafe(TargetActor), FinalDamage, StaminaCost);
		}
		else
		{
			// 가드 붕괴: 남은 스태미나를 모두 잃고 경직
			FinalPoiseDamage = GuardBreakPoiseDamage;
			if (TargetStamina > 0.0f)
			{
				OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UODAttributeSet_Combat::GetStaminaAttribute(), EGameplayModOp::Additive, -TargetStamina));
			}
			UE_LOG(LogODCombat, Log, TEXT("가드 붕괴: %s (스태미나 부족 %.1f < %.1f)"), *GetNameSafe(TargetActor), TargetStamina, StaminaCost);
		}
	}

	if (FinalDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UODAttributeSet_Health::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
	}

	if (FinalPoiseDamage > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UODAttributeSet_Health::GetIncomingPoiseDamageAttribute(), EGameplayModOp::Additive, FinalPoiseDamage));
	}
}
