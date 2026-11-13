// OnlyDungeon 장비 관리 컴포넌트

#include "ODEquipmentComponent.h"

#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Character/ODCharacterBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ODLogChannels.h"
#include "Engine/World.h"
#include "Equipment/ODWeaponActor.h"
#include "Equipment/ODWeaponData.h"

UODEquipmentComponent::UODEquipmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UODEquipmentComponent::EquipDefaultWeapon()
{
	if (IsValid(DefaultWeapon))
	{
		EquipWeapon(DefaultWeapon);
	}
}

void UODEquipmentComponent::EquipWeapon(UODWeaponData* NewWeaponData)
{
	AODCharacterBase* Character = Cast<AODCharacterBase>(GetOwner());
	UWorld* World = GetWorld();
	if (!IsValid(NewWeaponData) || !IsValid(Character) || !IsValid(World))
	{
		return;
	}

	UnequipWeapon();

	// 1) 무기 액터 스폰·부착
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const TSubclassOf<AODWeaponActor> ActorClass = NewWeaponData->WeaponActorClass ? NewWeaponData->WeaponActorClass : TSubclassOf<AODWeaponActor>(AODWeaponActor::StaticClass());
	WeaponActor = World->SpawnActor<AODWeaponActor>(ActorClass, Character->GetActorTransform(), SpawnParams);
	if (IsValid(WeaponActor))
	{
		WeaponActor->InitializeFromData(NewWeaponData);
		WeaponActor->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NewWeaponData->AttachSocket);
		WeaponActor->SetActorRelativeTransform(NewWeaponData->AttachOffset);
	}

	// 2) 어빌리티 세트와 무기 타입 태그
	if (UODAbilitySystemComponent* ASC = Character->GetODAbilitySystemComponent(); IsValid(ASC))
	{
		if (IsValid(NewWeaponData->AbilitySet))
		{
			NewWeaponData->AbilitySet->GiveToAbilitySystem(ASC, &WeaponAbilityHandles, NewWeaponData);
		}
		if (NewWeaponData->WeaponType.IsValid())
		{
			ASC->AddLooseGameplayTag(NewWeaponData->WeaponType);
		}
	}

	// 3) 이동 애니메이션 레이어
	if (NewWeaponData->AnimLayerClass)
	{
		Character->GetMesh()->LinkAnimClassLayers(NewWeaponData->AnimLayerClass);
	}

	WeaponData = NewWeaponData;
	UE_LOG(LogODCombat, Log, TEXT("%s 무기 장착: %s"), *GetNameSafe(Character), *GetNameSafe(NewWeaponData));
}

void UODEquipmentComponent::SetAbilityAttachment(bool bAbilityAttach)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(WeaponActor) || !IsValid(WeaponData) || !IsValid(Character) || WeaponData->AbilityAttachSocket.IsNone())
	{
		return;
	}

	const FName Socket = bAbilityAttach ? WeaponData->AbilityAttachSocket : WeaponData->AttachSocket;
	const FTransform& Offset = bAbilityAttach ? WeaponData->AbilityAttachOffset : WeaponData->AttachOffset;
	WeaponActor->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	WeaponActor->SetActorRelativeTransform(Offset);
}

void UODEquipmentComponent::UnequipWeapon()
{
	if (!IsValid(WeaponData))
	{
		return;
	}

	AODCharacterBase* Character = Cast<AODCharacterBase>(GetOwner());
	if (IsValid(Character))
	{
		if (UODAbilitySystemComponent* ASC = Character->GetODAbilitySystemComponent(); IsValid(ASC))
		{
			WeaponAbilityHandles.TakeFromAbilitySystem(ASC);
			if (WeaponData->WeaponType.IsValid())
			{
				ASC->RemoveLooseGameplayTag(WeaponData->WeaponType);
			}
		}

		if (WeaponData->AnimLayerClass && IsValid(Character->GetMesh()))
		{
			Character->GetMesh()->UnlinkAnimClassLayers(WeaponData->AnimLayerClass);
		}
	}

	if (IsValid(WeaponActor))
	{
		WeaponActor->Destroy();
	}

	WeaponActor = nullptr;
	WeaponData = nullptr;
}

void UODEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(WeaponActor))
	{
		WeaponActor->Destroy();
	}
	WeaponActor = nullptr;

	Super::EndPlay(EndPlayReason);
}
