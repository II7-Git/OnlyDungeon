// OnlyDungeon 캐릭터 공통 기반

#include "ODCharacterBase.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Combat.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponData.h"

AODCharacterBase::AODCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 이벤트 주도 구조: 캐릭터 액터 자체는 틱하지 않는다. (이동 컴포넌트는 자체 틱 유지)
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UODAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 액터의 하위 오브젝트로 만든 AttributeSet은 ASC 초기화 시 자동으로 등록된다.
	HealthSet = CreateDefaultSubobject<UODAttributeSet_Health>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<UODAttributeSet_Combat>(TEXT("CombatSet"));

	EquipmentComponent = CreateDefaultSubobject<UODEquipmentComponent>(TEXT("EquipmentComponent"));

	// 무기 판정이 화면 밖에서도 정확하도록, 보이지 않아도 본 위치를 갱신한다.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

UAbilitySystemComponent* AODCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FText AODCharacterBase::GetDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	FString ClassName = GetClass()->GetName();
	ClassName.RemoveFromEnd(TEXT("_C"));
	return FText::FromString(ClassName);
}

bool AODCharacterBase::IsDead() const
{
	return IsValid(AbilitySystemComponent) && AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Dead);
}

void AODCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystem();
}

void AODCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(HealthSet))
	{
		HealthSet->OnOutOfHealth.RemoveAll(this);
		HealthSet->OnPoiseBroken.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AODCharacterBase::InitializeAbilitySystem()
{
	if (bAbilitySystemInitialized || !IsValid(AbilitySystemComponent))
	{
		return;
	}
	bAbilitySystemInitialized = true;

	// 소유자와 아바타 모두 이 캐릭터 (ASC를 캐릭터에 두는 구조)
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (IsValid(HealthSet))
	{
		HealthSet->OnOutOfHealth.AddUObject(this, &ThisClass::HandleOutOfHealth);
		HealthSet->OnPoiseBroken.AddUObject(this, &ThisClass::HandlePoiseBroken);
	}

	if (IsValid(DefaultAbilitySet))
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &DefaultAbilitySetHandles, this);
	}
	else
	{
		UE_LOG(LogODAbility, Verbose, TEXT("%s: DefaultAbilitySet이 지정되지 않음"), *GetNameSafe(this));
	}

	// 무기 어빌리티 부여에 ASC가 필요하므로 초기화가 끝난 뒤 기본 무기를 장착한다.
	if (IsValid(EquipmentComponent))
	{
		EquipmentComponent->EquipDefaultWeapon();
	}
}

void AODCharacterBase::ResetForTest()
{
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent->CancelAllAbilities();
	AbilitySystemComponent->ClearAbilityInput();
	AbilitySystemComponent->SetLooseGameplayTagCount(ODGameplayTags::State_Dead, 0);

	// 기본 세트를 회수 후 다시 부여: 초기 수치 GE가 다시 적용되고 패시브(스태미나 회복)도 다시 발동한다.
	DefaultAbilitySetHandles.TakeFromAbilitySystem(AbilitySystemComponent);
	if (IsValid(DefaultAbilitySet))
	{
		DefaultAbilitySet->GiveToAbilitySystem(AbilitySystemComponent, &DefaultAbilitySetHandles, this);
	}

	// 무기 어빌리티도 다시 부여
	if (IsValid(EquipmentComponent))
	{
		if (UODWeaponData* Weapon = EquipmentComponent->GetWeaponData())
		{
			EquipmentComponent->EquipWeapon(Weapon);
		}
		else
		{
			EquipmentComponent->EquipDefaultWeapon();
		}
	}

	UE_LOG(LogOD, Log, TEXT("%s 테스트 초기화"), *GetNameSafe(this));
}

void AODCharacterBase::HandleOutOfHealth(AActor* DamageInstigator)
{
	if (IsDead())
	{
		return;
	}

	UE_LOG(LogODCombat, Log, TEXT("%s 사망 (원인: %s)"), *GetNameSafe(this), *GetNameSafe(DamageInstigator));

	AbilitySystemComponent->AddLooseGameplayTag(ODGameplayTags::State_Dead);
	AbilitySystemComponent->ClearAbilityInput();
	AbilitySystemComponent->CancelAllAbilities();

	FGameplayEventData Payload;
	Payload.EventTag = ODGameplayTags::Event_Death;
	Payload.Instigator = DamageInstigator;
	Payload.Target = this;
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);

	OnDeath.Broadcast(this);
}

void AODCharacterBase::HandlePoiseBroken(AActor* DamageInstigator)
{
	UE_LOG(LogODCombat, Verbose, TEXT("%s 강인도 붕괴 (원인: %s)"), *GetNameSafe(this), *GetNameSafe(DamageInstigator));

	// 경직 처리는 이 이벤트를 받는 어빌리티가 담당한다. (이후 단계)
	FGameplayEventData Payload;
	Payload.EventTag = ODGameplayTags::Event_PoiseBroken;
	Payload.Instigator = DamageInstigator;
	Payload.Target = this;
	AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);
}
