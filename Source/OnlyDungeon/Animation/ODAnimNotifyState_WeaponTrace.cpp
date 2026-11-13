// OnlyDungeon 애님 노티파이 스테이트: 무기 판정 구간

#include "ODAnimNotifyState_WeaponTrace.h"

#include "Components/SkeletalMeshComponent.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponActor.h"

UODAnimNotifyState_WeaponTrace::UODAnimNotifyState_WeaponTrace()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(220, 60, 60);
#endif
}

void UODAnimNotifyState_WeaponTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (AODWeaponActor* Weapon = FindWeapon(MeshComp))
	{
		Weapon->BeginTrace();
	}
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
