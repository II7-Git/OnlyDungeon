// OnlyDungeon 장비 관리 컴포넌트

#include "ODEquipmentComponent.h"

#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Character/ODCharacterBase.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

	// 왼손 장식 메시 (판정 없음)
	if (IsValid(NewWeaponData->OffhandMesh) && IsValid(Character->GetMesh()))
	{
		OffhandMeshComponent = NewObject<UStaticMeshComponent>(Character, NAME_None, RF_Transient);
		OffhandMeshComponent->SetStaticMesh(NewWeaponData->OffhandMesh);
		OffhandMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OffhandMeshComponent->SetGenerateOverlapEvents(false);
		OffhandMeshComponent->SetCanEverAffectNavigation(false);
		OffhandMeshComponent->SetupAttachment(Character->GetMesh(), NewWeaponData->OffhandAttachSocket);
		OffhandMeshComponent->SetRelativeTransform(NewWeaponData->OffhandAttachOffset);
		OffhandMeshComponent->RegisterComponent();
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

	// 3) 무기별 애님 블루프린트 (대기·이동 모션)
	if (NewWeaponData->AnimInstanceClass)
	{
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (!DefaultAnimInstanceClass)
		{
			DefaultAnimInstanceClass = Mesh->GetAnimClass();
		}
		RequestAnimClass(NewWeaponData->AnimInstanceClass);
	}

	// 4) 이동 애니메이션 레이어
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

		// 무기 애님 블루프린트를 썼다면 기본으로 되돌린다.
		if (WeaponData->AnimInstanceClass && DefaultAnimInstanceClass)
		{
			RequestAnimClass(DefaultAnimInstanceClass);
		}
	}

	if (IsValid(WeaponActor))
	{
		WeaponActor->Destroy();
	}
	if (IsValid(OffhandMeshComponent))
	{
		OffhandMeshComponent->DestroyComponent();
	}
	OffhandMeshComponent = nullptr;

	WeaponActor = nullptr;
	WeaponData = nullptr;
}

void UODEquipmentComponent::RequestAnimClass(TSubclassOf<UAnimInstance> NewClass)
{
	UWorld* World = GetWorld();
	if (!NewClass || !IsValid(World))
	{
		return;
	}

	PendingAnimClass = NewClass;
	UnbindMontageWait();
	if (!World->GetTimerManager().IsTimerActive(PendingAnimClassTimer))
	{
		PendingAnimClassTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::TryApplyPendingAnimClass);
	}
}

void UODEquipmentComponent::TryApplyPendingAnimClass()
{
	PendingAnimClassTimer.Invalidate();

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
	if (!PendingAnimClass || !IsValid(Mesh))
	{
		PendingAnimClass = nullptr;
		return;
	}

	if (Mesh->GetAnimClass() == PendingAnimClass)
	{
		PendingAnimClass = nullptr;
		return;
	}

	// 애님 인스턴스를 바꾸면 재생 중인 몽타주(무기 교체 등)가 끊기므로 모두 끝난 뒤 적용한다.
	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (IsValid(AnimInstance) && AnimInstance->IsAnyMontagePlaying())
	{
		if (MontageWaitInstance.Get() != AnimInstance)
		{
			UnbindMontageWait();
			AnimInstance->OnAllMontageInstancesEnded.AddUniqueDynamic(this, &ThisClass::HandleAllMontagesEnded);
			MontageWaitInstance = AnimInstance;
		}
		return;
	}

	UnbindMontageWait();
	Mesh->SetAnimInstanceClass(PendingAnimClass);
	UE_LOG(LogODCombat, Verbose, TEXT("%s 애님 블루프린트 적용: %s"), *GetNameSafe(GetOwner()), *GetNameSafe(PendingAnimClass.Get()));
	PendingAnimClass = nullptr;
}

void UODEquipmentComponent::HandleAllMontagesEnded()
{
	UnbindMontageWait();

	UWorld* World = GetWorld();
	if (PendingAnimClass && IsValid(World) && !World->GetTimerManager().IsTimerActive(PendingAnimClassTimer))
	{
		PendingAnimClassTimer = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::TryApplyPendingAnimClass);
	}
}

void UODEquipmentComponent::UnbindMontageWait()
{
	if (UAnimInstance* Waiting = MontageWaitInstance.Get())
	{
		Waiting->OnAllMontageInstancesEnded.RemoveDynamic(this, &ThisClass::HandleAllMontagesEnded);
	}
	MontageWaitInstance.Reset();
}

void UODEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingAnimClassTimer);
	}
	UnbindMontageWait();
	PendingAnimClass = nullptr;

	if (IsValid(WeaponActor))
	{
		WeaponActor->Destroy();
	}
	WeaponActor = nullptr;

	Super::EndPlay(EndPlayReason);
}
