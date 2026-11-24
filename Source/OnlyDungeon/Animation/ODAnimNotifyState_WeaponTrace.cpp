// OnlyDungeon 애님 노티파이 스테이트: 무기 판정 구간

#include "ODAnimNotifyState_WeaponTrace.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/ODCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ODGameplayTags.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponActor.h"
#include "Equipment/ODWeaponData.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

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
		PlaySwingSound(Weapon);
		Weapon->StartTrail();

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
		Weapon->StopTrail();
	}
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	SendHitTraceEvent(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner), Owner, 0.0f);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UODAnimNotifyState_WeaponTrace::GetNotifyName_Implementation() const
{
	return TEXT("WeaponTrace");
}

void UODAnimNotifyState_WeaponTrace::PlaySwingSound(AODWeaponActor* Weapon)
{
	const AActor* Owner = Weapon->GetOwner();
	const AODCharacterBase* OwnerCharacter = Cast<AODCharacterBase>(Owner);
	const UODEquipmentComponent* Equipment = OwnerCharacter ? OwnerCharacter->GetEquipmentComponent() : nullptr;
	const UODWeaponData* Data = IsValid(Equipment) ? Equipment->GetWeaponData() : nullptr;
	if (IsValid(Data) && IsValid(Data->SwingSound) && IsValid(Weapon->GetMeshComponent()))
	{
		// 무기에 붙여 재생해 칼끝이 움직이는 방향에서 소리가 난다. 무기가 사라지면 함께 멈춘다.
		UGameplayStatics::SpawnSoundAttached(Data->SwingSound, Weapon->GetMeshComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true);
	}
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

	// NotifyTick에서 매 프레임 불리므로 캐릭터의 게터로 O(1) 조회한다. 캐릭터가 아닌 소유자(에디터 미리보기 등)만 컴포넌트 검색으로 대체한다.
	const AODCharacterBase* OwnerCharacter = Cast<AODCharacterBase>(Owner);
	const UODEquipmentComponent* Equipment = OwnerCharacter ? OwnerCharacter->GetEquipmentComponent() : Owner->FindComponentByClass<UODEquipmentComponent>();
	if (!IsValid(Equipment))
	{
		return nullptr;
	}

	AODWeaponActor* Weapon = Equipment->GetWeaponActor();
	return IsValid(Weapon) ? Weapon : nullptr;
}
