// OnlyDungeon 무기 교체 어빌리티 (Tab)

#include "ODGameplayAbility_SwapWeapon.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponActor.h"
#include "Equipment/ODWeaponData.h"

UODGameplayAbility_SwapWeapon::UODGameplayAbility_SwapWeapon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 물약처럼 행동(Ability.Action)이 아니라 장비 조작이다. 교체하는 동안 공격·회피를 막는다.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(ODGameplayTags::Ability_Item);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(ODGameplayTags::State_SwappingWeapon);
	BlockAbilitiesWithTag.AddTag(ODGameplayTags::Ability_Action);
	BlockAbilitiesWithTag.AddTag(ODGameplayTags::Ability_Item);

	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Attacking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Dodging);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Defending);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Drinking);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_Resting);
	ActivationBlockedTags.AddTag(ODGameplayTags::State_SwappingWeapon);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ODGameplayTags::Event_Weapon_SwapRequest;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

bool UODGameplayAbility_SwapWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && IsValid(SwapMontage);
}

void UODGameplayAbility_SwapWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const AODCharacterBase* Character = GetODCharacterFromActorInfo();
	UODWeaponData* NextWeapon = TriggerEventData ? const_cast<UODWeaponData*>(Cast<UODWeaponData>(TriggerEventData->OptionalObject.Get())) : nullptr;
	const UODEquipmentComponent* Equipment = IsValid(Character) ? Character->GetEquipmentComponent() : nullptr;
	if (!IsValid(NextWeapon) || !IsValid(Equipment) || Equipment->GetWeaponData() == NextWeapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogODAbility, Verbose, TEXT("[%s] 무기 교체를 시작하지 않습니다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PendingWeapon = NextWeapon;
	bSwapped = false;

	// 경직·사망하면 교체를 취소한다. (상체 몽타주라 피격 몽타주와 겹쳐 계속 재생될 수 있다)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		StunnedHandle = ASC->RegisterGameplayTagEvent(ODGameplayTags::State_Stunned).AddUObject(this, &ThisClass::OnInterruptTagChanged);
		DeadHandle = ASC->RegisterGameplayTagEvent(ODGameplayTags::State_Dead).AddUObject(this, &ThisClass::OnInterruptTagChanged);
	}

	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ODGameplayTags::Event_Montage_WeaponSwap, nullptr, false, true);
	EventTask->EventReceived.AddDynamic(this, &ThisClass::OnSwapEvent);
	EventTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, SwapMontage, MontagePlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->ReadyForActivation();
}

void UODGameplayAbility_SwapWeapon::OnSwapEvent(FGameplayEventData Payload)
{
	if (!IsActive())
	{
		return;
	}

	// 구간 열림(1): 손이 등에 닿음 → 지금 무기를 넣고 새 무기로 바꾼 뒤, 꺼낼 때까지 숨긴다.
	if (Payload.EventMagnitude > 0.5f)
	{
		AODCharacterBase* Character = GetODCharacterFromActorInfo();
		UODEquipmentComponent* Equipment = IsValid(Character) ? Character->GetEquipmentComponent() : nullptr;
		UODWeaponData* NextWeapon = PendingWeapon.Get();
		if (!bSwapped && IsValid(Equipment) && IsValid(NextWeapon))
		{
			bSwapped = true;
			Equipment->EquipWeapon(NextWeapon);
			SetWeaponHidden(true);
			UE_LOG(LogODAbility, Log, TEXT("[%s] 무기 교체: %s"), *GetName(), *GetNameSafe(NextWeapon));
		}
		return;
	}

	// 구간 닫힘(0): 등에서 칼을 쥠 → 새 무기를 보인다.
	SetWeaponHidden(false);
}

void UODGameplayAbility_SwapWeapon::SetWeaponHidden(bool bHidden)
{
	if (!bHidden)
	{
		if (AActor* Weapon = HiddenWeapon.Get())
		{
			Weapon->SetActorHiddenInGame(false);
		}
		HiddenWeapon.Reset();
		return;
	}

	const AODCharacterBase* Character = GetODCharacterFromActorInfo();
	const UODEquipmentComponent* Equipment = IsValid(Character) ? Character->GetEquipmentComponent() : nullptr;
	if (AODWeaponActor* Weapon = IsValid(Equipment) ? Equipment->GetWeaponActor() : nullptr; IsValid(Weapon))
	{
		Weapon->SetActorHiddenInGame(true);
		HiddenWeapon = Weapon;
	}
}

void UODGameplayAbility_SwapWeapon::OnInterruptTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0 && IsActive())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

void UODGameplayAbility_SwapWeapon::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UODGameplayAbility_SwapWeapon::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 어떤 경로로 끝나도(꺼내기 전에 끊겨도) 무기를 손에 보이게 되돌린다. 이미 바꿨으면 새 무기를 그대로 든다.
	SetWeaponHidden(false);
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RegisterGameplayTagEvent(ODGameplayTags::State_Stunned).Remove(StunnedHandle);
		ASC->RegisterGameplayTagEvent(ODGameplayTags::State_Dead).Remove(DeadHandle);
	}
	StunnedHandle.Reset();
	DeadHandle.Reset();
	PendingWeapon.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
