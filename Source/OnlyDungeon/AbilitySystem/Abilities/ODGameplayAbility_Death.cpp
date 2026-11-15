// OnlyDungeon 사망 어빌리티

#include "ODGameplayAbility_Death.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

UODGameplayAbility_Death::UODGameplayAbility_Death(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 입력이 아니라 사망 이벤트로 발동한다.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FAbilityTriggerData Trigger;
		Trigger.TriggerTag = ODGameplayTags::Event_Death;
		Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
		AbilityTriggers.Add(Trigger);
	}
}

void UODGameplayAbility_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!IsValid(DeathMontage))
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 사망 몽타주가 지정되지 않았습니다."), *GetName());
		return;
	}

	// 몽타주가 끝나도 어빌리티는 부활 때까지 유지한다. (마지막 자세 유지)
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DeathMontage, MontagePlayRate);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
	MontageTask->ReadyForActivation();

	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 사망 연출 시작"), *GetName());
}

void UODGameplayAbility_Death::OnMontageInterrupted()
{
	// 다른 몽타주에 밀려난 경우에만 끝낸다. (부활 시 취소는 엔진이 정리)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
