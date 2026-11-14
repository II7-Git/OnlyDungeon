// OnlyDungeon 방어 어빌리티 (패링 / 가드)

#include "ODGameplayAbility_Defense.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "Equipment/ODEquipmentComponent.h"
#include "TimerManager.h"

UODGameplayAbility_Defense::UODGameplayAbility_Defense(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EODAbilityActivationPolicy::OnInputTriggered;
}

void UODGameplayAbility_Defense::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!IsValid(DefenseMontage))
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 방어 몽타주가 지정되지 않았습니다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 스태미나 비용 (부족하면 실패)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bRotateToInput)
	{
		AODCharacterBase* Character = GetODCharacterFromActorInfo();
		const FVector AimDirection = GetAimDirection();
		if (IsValid(Character) && !AimDirection.IsZero())
		{
			Character->SetActorRotation(AimDirection.Rotation());
		}
	}

	// 무기를 공격용 부착 위치로 (무기 데이터에 지정된 경우만)
	if (AODCharacterBase* OwnerCharacter = GetODCharacterFromActorInfo(); IsValid(OwnerCharacter) && IsValid(OwnerCharacter->GetEquipmentComponent()))
	{
		OwnerCharacter->GetEquipmentComponent()->SetAbilityAttachment(true);
	}

	ListenForCancelWindow();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DefenseMontage, 1.0f);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();

	if (DefenseType == EODDefenseType::Parry && ParryWindowDuration > 0.0f)
	{
		if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar) && ParryWindowStart > 0.0f)
		{
			Avatar->GetWorldTimerManager().SetTimer(ParryStartHandle, this, &ThisClass::BeginParryWindow, ParryWindowStart, false);
		}
		else
		{
			BeginParryWindow();
		}
	}

	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 방어 시작 (%s)"), *GetName(), *UEnum::GetValueAsString(DefenseType));
}

void UODGameplayAbility_Defense::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AODCharacterBase* OwnerCharacter = GetODCharacterFromActorInfo(); IsValid(OwnerCharacter) && IsValid(OwnerCharacter->GetEquipmentComponent()))
	{
		OwnerCharacter->GetEquipmentComponent()->SetAbilityAttachment(false);
	}

	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		Avatar->GetWorldTimerManager().ClearTimer(ParryStartHandle);
		Avatar->GetWorldTimerManager().ClearTimer(ParryEndHandle);
	}
	EndParryWindow();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UODGameplayAbility_Defense::BeginParryWindow()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (bParryWindowApplied || !IsValid(ASC))
	{
		return;
	}

	ASC->AddLooseGameplayTag(ODGameplayTags::State_Parrying);
	bParryWindowApplied = true;

	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		Avatar->GetWorldTimerManager().SetTimer(ParryEndHandle, this, &ThisClass::EndParryWindow, ParryWindowDuration, false);
	}
}

void UODGameplayAbility_Defense::EndParryWindow()
{
	if (!bParryWindowApplied)
	{
		return;
	}
	bParryWindowApplied = false;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); IsValid(ASC))
	{
		ASC->RemoveLooseGameplayTag(ODGameplayTags::State_Parrying);
	}
}

void UODGameplayAbility_Defense::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UODGameplayAbility_Defense::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
