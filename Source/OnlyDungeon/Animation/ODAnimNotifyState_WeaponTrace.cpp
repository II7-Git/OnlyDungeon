// OnlyDungeon 애님 노티파이 스테이트: 무기 판정 구간

#include "ODAnimNotifyState_WeaponTrace.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ODGameplayTags.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponActor.h"

namespace
{
	/** 판정 구간의 시작(1)·끝(0)을 소유자 ASC에 알린다. (필드 모드의 판정 직후 캔슬 등) */
	void SendHitTraceEvent(UAbilitySystemComponent* ASC, AActor* Owner, float Magnitude)
	{
		if (!IsValid(ASC))
		{
			return;
		}
		FGameplayEventData Payload;
		Payload.EventTag = ODGameplayTags::Event_Montage_HitTrace;
		Payload.Instigator = Owner;
		Payload.Target = Owner;
		Payload.EventMagnitude = Magnitude;
		ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	}
}

UODAnimNotifyState_WeaponTrace::UODAnimNotifyState_WeaponTrace()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 60, 60);
#endif
}

void UODAnimNotifyState_WeaponTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);

	if (AODWeaponActor* Weapon = FindWeapon(MeshComp))
	{
		// 필드(쿼터뷰): 다수의 적을 상대하므로 칼날 스윕 대신 전방 부채꼴 판정 1회
		if (IsValid(ASC) && ASC->HasMatchingGameplayTag(ODGameplayTags::Mode_Field))
		{
			Weapon->PerformAreaHit();
		}
		else
		{
			Weapon->BeginTrace();
		}
	}
	SendHitTraceEvent(ASC, Owner, 1.0f);
}

void UODAnimNotifyState_WeaponTrace::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (AODWeaponActor* Weapon = FindWeapon(MeshComp))
	{
		Weapon->TickTrace();
	}
}

void UODAnimNotifyState_WeaponTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (AODWeaponActor* Weapon = FindWeapon(MeshComp))
	{
		Weapon->EndTrace();
	}
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	SendHitTraceEvent(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner), Owner, 0.0f);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UODAnimNotifyState_WeaponTrace::GetNotifyName_Implementation() const
{
	return TEXT("WeaponTrace");
}

AODWeaponActor* UODAnimNotifyState_WeaponTrace::FindWeapon(const USkeletalMeshComponent* MeshComp)
{
	if (!IsValid(MeshComp))
	{
		return nullptr;
	}

	const AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	const UODEquipmentComponent* Equipment = Owner->FindComponentByClass<UODEquipmentComponent>();
	if (!IsValid(Equipment))
	{
		return nullptr;
	}

	AODWeaponActor* Weapon = Equipment->GetWeaponActor();
	return IsValid(Weapon) ? Weapon : nullptr;
}
