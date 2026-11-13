// OnlyDungeon 회피(구르기) 어빌리티

#include "ODGameplayAbility_Dodge.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/ODCharacterBase.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "TimerManager.h"

UODGameplayAbility_Dodge::UODGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = EODAbilityActivationPolicy::OnInputTriggered;
}

void UODGameplayAbility_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AODCharacterBase* Character = GetODCharacterFromActorInfo();
	if (!IsValid(Character))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector InputDirection = GetMovementInputDirection(InputDeadZone);
	AActor* LockOnTarget = Character->GetLockOnTarget();
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const bool bLockedOn = IsValid(LockOnTarget) && IsValid(ASC) && ASC->HasMatchingGameplayTag(ODGameplayTags::State_LockOn);

	// 방향 결정과 회전
	EODDodgeDirection Direction = EODDodgeDirection::Front;
	if (bLockedOn)
	{
		// 락온: 대상을 바라보고, 대상 기준 입력 방향으로 8방향 구르기 (입력 없으면 뒤로)
		FVector ToTarget = LockOnTarget->GetActorLocation() - Character->GetActorLocation();
		ToTarget.Z = 0.0f;
		if (!ToTarget.IsNearlyZero())
		{
			Character->SetActorRotation(ToTarget.Rotation());
		}
		Direction = InputDirection.IsZero() ? EODDodgeDirection::Back : SelectDirection(Character->GetActorForwardVector(), InputDirection);
	}
	else if (!InputDirection.IsZero())
	{
		// 비락온: 입력 방향으로 즉시 돌아서 정면 구르기
		Character->SetActorRotation(InputDirection.Rotation());
		Direction = EODDodgeDirection::Front;
	}
	else
	{
		// 입력 없음: 바라보는 방향 그대로 백스텝
		Direction = EODDodgeDirection::Back;
	}

	UAnimMontage* Montage = DodgeMontages[static_cast<uint8>(Direction)];
	if (!IsValid(Montage))
	{
		UE_LOG(LogODAbility, Warning, TEXT("[%s] 방향 %s의 구르기 몽타주가 지정되지 않았습니다."), *GetName(), *UEnum::GetValueAsString(Direction));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 스태미나 소모 (부족하면 실패)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ListenForCancelWindow();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Montage, MontagePlayRate);
	MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);
	MontageTask->ReadyForActivation();

	// 무적 구간 예약
	if (InvulnerableDuration > 0.0f)
	{
		FTimerManager& TimerManager = Character->GetWorldTimerManager();
		if (InvulnerableStartTime > 0.0f)
		{
			TimerManager.SetTimer(InvulnerableStartHandle, this, &ThisClass::BeginInvulnerable, InvulnerableStartTime, false);
		}
		else
		{
			BeginInvulnerable();
		}
	}

	UE_LOG(LogODAbility, Verbose, TEXT("[%s] 회피 시작: %s (락온 %d)"), *GetName(), *UEnum::GetValueAsString(Direction), bLockedOn ? 1 : 0);
}

void UODGameplayAbility_Dodge::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		FTimerManager& TimerManager = Avatar->GetWorldTimerManager();
		TimerManager.ClearTimer(InvulnerableStartHandle);
		TimerManager.ClearTimer(InvulnerableEndHandle);
	}
	EndInvulnerable();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

EODDodgeDirection UODGameplayAbility_Dodge::SelectDirection(const FVector& Forward, const FVector& InputDirection)
{
	// 정면 기준 각도: 오른쪽이 양수 (-180 ~ 180)
	const FVector Forward2D = Forward.GetSafeNormal2D();
	const FVector Right2D = FVector::CrossProduct(FVector::UpVector, Forward2D);
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(InputDirection, Right2D), FVector::DotProduct(InputDirection, Forward2D)));

	// 45도 단위로 반올림해 시계 방향 인덱스로 변환
	const int32 Index = (FMath::RoundToInt(Angle / 45.0f) + 8) % 8;
	return static_cast<EODDodgeDirection>(Index);
}

void UODGameplayAbility_Dodge::BeginInvulnerable()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (bInvulnerableApplied || !IsValid(ASC))
	{
		return;
	}

	ASC->AddLooseGameplayTag(ODGameplayTags::State_Invulnerable);
	bInvulnerableApplied = true;

	if (const AActor* Avatar = GetAvatarActorFromActorInfo(); IsValid(Avatar))
	{
		Avatar->GetWorldTimerManager().SetTimer(InvulnerableEndHandle, this, &ThisClass::EndInvulnerable, InvulnerableDuration, false);
	}
}

void UODGameplayAbility_Dodge::EndInvulnerable()
{
	if (!bInvulnerableApplied)
	{
		return;
	}
	bInvulnerableApplied = false;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo(); IsValid(ASC))
	{
		ASC->RemoveLooseGameplayTag(ODGameplayTags::State_Invulnerable);
	}
}

void UODGameplayAbility_Dodge::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UODGameplayAbility_Dodge::OnMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
