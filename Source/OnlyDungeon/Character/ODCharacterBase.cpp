// OnlyDungeon 캐릭터 공통 기반

#include "ODCharacterBase.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Health.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Combat.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

AODCharacterBase::AODCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 이벤트 주도 구조: 캐릭터 액터 자체는 틱하지 않는다. (이동 컴포넌트는 자체 틱 유지)
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UODAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	// 액터의 하위 오브젝트로 만든 AttributeSet은 ASC 초기화 시 자동으로 등록된다.
	HealthSet = CreateDefaultSubobject<UODAttributeSet_Health>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<UODAttributeSet_Combat>(TEXT("CombatSet"));
}

UAbilitySystemComponent* AODCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
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
