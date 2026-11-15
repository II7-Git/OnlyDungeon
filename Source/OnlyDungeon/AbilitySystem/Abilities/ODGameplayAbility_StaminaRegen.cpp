// OnlyDungeon 스태미나 자연 회복 패시브 어빌리티

#include "ODGameplayAbility_StaminaRegen.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ODAttributeSet_Combat.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

UODGameplayAbility_StaminaRegen::UODGameplayAbility_StaminaRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 부여되는 즉시 발동하고, 이후 계속 살아 있는 패시브
	ActivationPolicy = EODAbilityActivationPolicy::OnSpawn;
}

void UODGameplayAbility_StaminaRegen::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC) || !RegenEffectClass)
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] ASC 또는 회복 GE가 없어 스태미나 회복을 시작하지 못했습니다."), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UODAttributeSet_Combat::GetStaminaAttribute()).AddUObject(this, &ThisClass::HandleStaminaChanged);

	// 시작 시점에 스태미나가 비어 있을 수도 있으므로 대기부터 시작한다.
	RestartRegenDelay();
}

void UODGameplayAbility_StaminaRegen::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); IsValid(ASC) && StaminaChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UODAttributeSet_Combat::GetStaminaAttribute()).Remove(StaminaChangedHandle);
	}
	StaminaChangedHandle.Reset();

	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		Avatar->GetWorldTimerManager().ClearTimer(RegenDelayHandle);
	}
	StopRegen();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UODGameplayAbility_StaminaRegen::HandleStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.NewValue < ChangeData.OldValue)
	{
		// 소모 발생: 회복을 멈추고 대기 시간을 처음부터 다시 잰다.
		StopRegen();
		RestartRegenDelay();
	}
	else if (IsStaminaFull())
	{
		// 가득 참: 회복을 멈춘다. 다음 소모 때 다시 시작된다.
		StopRegen();
	}
}

void UODGameplayAbility_StaminaRegen::RestartRegenDelay()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return;
	}

	FTimerManager& TimerManager = Avatar->GetWorldTimerManager();
	TimerManager.ClearTimer(RegenDelayHandle);

	if (IsStaminaFull())
	{
		return;
	}

	if (RegenDelay > 0.0f)
	{
		TimerManager.SetTimer(RegenDelayHandle, this, &ThisClass::StartRegen, RegenDelay, false);
	}
	else
	{
		StartRegen();
	}
}

void UODGameplayAbility_StaminaRegen::StartRegen()
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (bRegenActive || IsStaminaFull() || !IsValid(Avatar))
	{
		return;
	}

	bRegenActive = true;
	RegenElapsed = 0.0f;
	Avatar->GetWorldTimerManager().SetTimer(RegenTickHandle, this, &ThisClass::ApplyRegenTick, TickInterval, true);
	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 스태미나 회복 시작"), *GetName());
}

void UODGameplayAbility_StaminaRegen::StopRegen()
{
	if (!bRegenActive)
	{
		return;
	}

	bRegenActive = false;
	RegenElapsed = 0.0f;
	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		Avatar->GetWorldTimerManager().ClearTimer(RegenTickHandle);
	}
	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 스태미나 회복 정지"), *GetName());
}

float UODGameplayAbility_StaminaRegen::GetCurrentRateMultiplier() const
{
	const float Alpha = RampTime > 0.0f ? FMath::Clamp(RegenElapsed / RampTime, 0.0f, 1.0f) : 1.0f;
	return FMath::Lerp(StartRateMultiplier, MaxRateMultiplier, Alpha);
}

void UODGameplayAbility_StaminaRegen::ApplyRegenTick()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!bRegenActive || !IsValid(ASC) || IsStaminaFull())
	{
		StopRegen();
		return;
	}

	RegenElapsed += TickInterval;
	const float Rate = ASC->GetNumericAttribute(UODAttributeSet_Combat::GetStaminaRegenRateAttribute());
	const float Amount = Rate * GetCurrentRateMultiplier() * TickInterval;
	if (Amount <= 0.0f)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RegenEffectClass, GetAbilityLevel());
	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(ODGameplayTags::Data_StaminaRegen, Amount);
		// 이 적용으로 스태미나가 오르면 HandleStaminaChanged가 가득 참을 확인해 회복을 멈춘다.
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
	}
}

bool UODGameplayAbility_StaminaRegen::IsStaminaFull() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(ASC))
	{
		return true;
	}

	const float Stamina = ASC->GetNumericAttribute(UODAttributeSet_Combat::GetStaminaAttribute());
	const float MaxStamina = ASC->GetNumericAttribute(UODAttributeSet_Combat::GetMaxStaminaAttribute());
	return Stamina >= MaxStamina;
}
