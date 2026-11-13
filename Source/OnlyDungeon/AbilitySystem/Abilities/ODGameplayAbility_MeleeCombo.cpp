// OnlyDungeon 근접 콤보 공격 어빌리티 (약공격 등)

#include "ODGameplayAbility_MeleeCombo.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "Equipment/ODEquipmentComponent.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

UODGameplayAbility_MeleeCombo::UODGameplayAbility_MeleeCombo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EODAbilityActivationPolicy::OnInputTriggered;
}

void UODGameplayAbility_MeleeCombo::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!IsValid(ComboMontage) || ComboSteps.IsEmpty())
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 콤보 몽타주 또는 섹션이 지정되지 않았습니다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 첫 타격 비용 (스태미나 부족하면 실패)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ComboIndex = 0;
	bComboWindowOpen = false;
	bComboQueued = false;

	if (bRotateToInput)
	{
		RotateToInputDirection();
	}

	UAbilityTask_WaitGameplayEvent* WindowTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ODGameplayTags::Event_Montage_ComboWindow, nullptr, false, true);
	WindowTask->EventReceived.AddDynamic(this, &ThisClass::OnComboWindowEvent);
	WindowTask->ReadyForActivation();

	ListenForCancelWindow();

	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ODGameplayTags::Event_Melee_Hit, nullptr, false, true);
	HitTask->EventReceived.AddDynamic(this, &ThisClass::OnMeleeHit);
	HitTask->ReadyForActivation();

	// 무기를 공격용 부착 위치로 (무기 데이터에 지정된 경우만)
	if (AODCharacterBase* OwnerCharacter = GetODCharacterFromActorInfo(); IsValid(OwnerCharacter) && IsValid(OwnerCharacter->GetEquipmentComponent()))
	{
		OwnerCharacter->GetEquipmentComponent()->SetAbilityAttachment(true);
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ComboMontage, MontagePlayRate, ComboSteps[0].Section);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();

	StartStepForward();

	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 콤보 시작: %s"), *GetName(), *ComboSteps[0].Section.ToString());
}

void UODGameplayAbility_MeleeCombo::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AODCharacterBase* OwnerCharacter = GetODCharacterFromActorInfo(); IsValid(OwnerCharacter) && IsValid(OwnerCharacter->GetEquipmentComponent()))
	{
		OwnerCharacter->GetEquipmentComponent()->SetAbilityAttachment(false);
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepDelayTimer);
	}
	// 태스크 자체는 어빌리티 종료 시 엔진이 정리한다.
	StepTask = nullptr;

	ComboIndex = 0;
	bComboWindowOpen = false;
	bComboQueued = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UODGameplayAbility_MeleeCombo::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);

	if (!IsActive())
	{
		return;
	}

	// 공격 중 입력은 예약해 두고, 콤보 구간이 열려 있으면 바로 다음 타격으로 넘어간다.
	bComboQueued = true;
	if (bComboWindowOpen)
	{
		TryAdvanceCombo();
	}
}

void UODGameplayAbility_MeleeCombo::OnComboWindowEvent(FGameplayEventData Payload)
{
	bComboWindowOpen = Payload.EventMagnitude > 0.0f;

	if (bComboWindowOpen)
	{
		// 구간이 열리기 전에 눌러 둔 입력 처리
		if (bComboQueued)
		{
			TryAdvanceCombo();
		}
	}
	else
	{
		// 구간이 닫힌 뒤의 입력은 다음 콤보로 이어지지 않는다.
		bComboQueued = false;
	}
}

void UODGameplayAbility_MeleeCombo::OnMeleeHit(FGameplayEventData Payload)
{
	if (!DamageEffectClass || !ComboSteps.IsValidIndex(ComboIndex))
	{
		return;
	}

	const AActor* TargetActor = Payload.Target.Get();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(TargetASC) || !IsValid(SourceASC) || TargetASC->HasMatchingGameplayTag(ODGameplayTags::State_Dead))
	{
		return;
	}

	const FODComboStep& Step = ComboSteps[ComboIndex];
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(ODGameplayTags::Data_Damage, Step.Damage);
	SpecHandle.Data->SetSetByCallerMagnitude(ODGameplayTags::Data_PoiseDamage, Step.PoiseDamage);
	if (const FHitResult* Hit = Payload.TargetData.Get(0) ? Payload.TargetData.Get(0)->GetHitResult() : nullptr)
	{
		SpecHandle.Data->GetContext().AddHitResult(*Hit, true);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
	UE_LOG(LogODCombat, Verbose, TEXT("[%s] %s 적중: %s (피해 %.0f, 강인도 %.0f)"), *GetName(), *Step.Section.ToString(), *GetNameSafe(TargetActor), Step.Damage, Step.PoiseDamage);
}

void UODGameplayAbility_MeleeCombo::TryAdvanceCombo()
{
	bComboQueued = false;

	const int32 NextIndex = ComboIndex + 1;
	if (!ComboSteps.IsValidIndex(NextIndex))
	{
		return;
	}

	// 매 타격마다 스태미나 비용 지불 (부족하면 콤보 종료 흐름 유지)
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogODAbility, Verbose, TEXT("[%s] 스태미나 부족으로 콤보 중단"), *GetName());
		return;
	}

	ComboIndex = NextIndex;
	bComboWindowOpen = false;
	ClearCancelWindow();

	if (bRotateToInput)
	{
		RotateToInputDirection();
	}

	MontageJumpToSection(ComboSteps[ComboIndex].Section);
	StartStepForward();
	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 콤보 진행: %s"), *GetName(), *ComboSteps[ComboIndex].Section.ToString());
}

void UODGameplayAbility_MeleeCombo::RotateToInputDirection() const
{
	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	if (!IsValid(Character))
	{
		return;
	}

	const FVector InputDirection = GetMovementInputDirection();
	if (!InputDirection.IsZero())
	{
		Character->SetActorRotation(InputDirection.Rotation());
	}
}

void UODGameplayAbility_MeleeCombo::StartStepForward()
{
	// 이전 타격의 전진이 끝나기 전에 다음 타격으로 넘어가면 끊고 새 방향으로 다시 민다.
	if (IsValid(StepTask))
	{
		StepTask->EndTask();
	}
	StepTask = nullptr;

	UWorld* World = GetWorld();
	if (!IsValid(World) || !ComboSteps.IsValidIndex(ComboIndex))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(StepDelayTimer);

	const FODComboStep& Step = ComboSteps[ComboIndex];
	if (Step.StepDistance <= 0.0f)
	{
		return;
	}

	if (Step.StepDelay > 0.0f)
	{
		World->GetTimerManager().SetTimer(StepDelayTimer, this, &ThisClass::ApplyStepForce, Step.StepDelay, false);
	}
	else
	{
		ApplyStepForce();
	}
}

void UODGameplayAbility_MeleeCombo::ApplyStepForce()
{
	if (!IsActive() || !ComboSteps.IsValidIndex(ComboIndex))
	{
		return;
	}

	const AODCharacterBase* Character = GetODCharacterFromActorInfo();
	if (!IsValid(Character))
	{
		return;
	}

	const FODComboStep& Step = ComboSteps[ComboIndex];
	const float Duration = FMath::Max(Step.StepDuration, 0.01f);
	const FVector Direction = Character->GetActorForwardVector().GetSafeNormal2D();

	// 비가산(덮어쓰기) 방식이라 이동 입력과 섞이지 않고, 끝나면 속도를 0으로 맞춰 미끄러지지 않게 한다.
	StepTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("ComboStep"), Direction, Step.StepDistance / Duration, Duration,
		false, nullptr, ERootMotionFinishVelocityMode::SetVelocity, FVector::ZeroVector, 0.0f, true);
	StepTask->ReadyForActivation();

	UE_LOG(LogODAbility, Verbose, TEXT("[%s] %s 전진 %.0fcm / %.2f초"), *GetName(), *Step.Section.ToString(), Step.StepDistance, Duration);
}

void UODGameplayAbility_MeleeCombo::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UODGameplayAbility_MeleeCombo::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
