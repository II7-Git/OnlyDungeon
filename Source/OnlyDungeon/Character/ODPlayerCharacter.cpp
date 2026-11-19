// OnlyDungeon 플레이어 캐릭터

#include "ODPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"
#include "Equipment/ODEquipmentComponent.h"
#include "Equipment/ODWeaponData.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Input/ODInputConfig.h"
#include "Character/ODCombatModeComponent.h"
#include "Character/ODLockOnComponent.h"
#include "Character/ODPotionComponent.h"
#include "Interaction/ODInteractionComponent.h"
#include "World/ODBossEntrance.h"
#include "Core/ODGameMode.h"
#include "TimerManager.h"

AODPlayerCharacter::AODPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// 컨트롤러 회전은 카메라에만 적용하고, 캐릭터는 이동 방향을 바라본다.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = true;
	MoveComp->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	MoveComp->MaxWalkSpeed = 500.0f;
	MoveComp->MinAnalogWalkSpeed = 20.0f;
	MoveComp->BrakingDecelerationWalking = 2000.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	CombatModeComponent = CreateDefaultSubobject<UODCombatModeComponent>(TEXT("CombatMode"));
	InteractionComponent = CreateDefaultSubobject<UODInteractionComponent>(TEXT("Interaction"));
	LockOnComponent = CreateDefaultSubobject<UODLockOnComponent>(TEXT("LockOn"));
	PotionComponent = CreateDefaultSubobject<UODPotionComponent>(TEXT("Potion"));

	// 플레이어 진영
	TeamId = 0;
}

void AODPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// 모드별 입력 매핑·카메라는 전투 모드 컴포넌트가 다시 맞춘다. (BeginPlay 시점에는 아직 컨트롤러가 없을 수 있음)
	if (IsValid(CombatModeComponent))
	{
		CombatModeComponent->ReapplyCurrentMode();
	}

	// 모드별 매핑이 없으면 기본 입력 매핑을 등록한다.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !IsValid(DefaultMappingContext))
	{
		return;
	}
	if (IsValid(CombatModeComponent) && IsValid(CombatModeComponent->GetSettings(CombatModeComponent->GetCombatMode()).InputMapping))
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (!Subsystem->HasMappingContext(DefaultMappingContext))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, DefaultMappingPriority);
		}
	}
}

void AODPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogOD, Error, TEXT("%s: Enhanced Input 컴포넌트를 찾지 못함"), *GetNameSafe(this));
		return;
	}

	if (!IsValid(InputConfig))
	{
		UE_LOG(LogOD, Warning, TEXT("%s: InputConfig가 지정되지 않아 입력을 연결하지 않음"), *GetNameSafe(this));
		return;
	}

	// 이동/시점: C++ 함수에 직접 연결
	if (const UInputAction* MoveAction = InputConfig->FindNativeInputActionForTag(ODGameplayTags::InputTag_Move))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
	}
	if (const UInputAction* LookAction = InputConfig->FindNativeInputActionForTag(ODGameplayTags::InputTag_Look))
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	}

	// [테스트] 무기 교체·상태 초기화 (액션이 설정에 없으면 연결하지 않음)
	if (const UInputAction* SwapAction = InputConfig->FindNativeInputActionForTag(ODGameplayTags::InputTag_Debug_SwapWeapon, false))
	{
		EIC->BindAction(SwapAction, ETriggerEvent::Started, this, &ThisClass::Input_Debug_SwapWeapon);
	}
	if (const UInputAction* ResetAction = InputConfig->FindNativeInputActionForTag(ODGameplayTags::InputTag_Debug_Reset, false))
	{
		EIC->BindAction(ResetAction, ETriggerEvent::Started, this, &ThisClass::Input_Debug_Reset);
	}

	// 상호작용 (문 등)
	if (const UInputAction* InteractAction = InputConfig->FindNativeInputActionForTag(ODGameplayTags::InputTag_Interact, false))
	{
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Input_Interact);
	}

	// 락온 전환
	if (const UInputAction* LockOnAction = InputConfig->FindNativeInputActionForTag(ODGameplayTags::InputTag_LockOn, false))
	{
		EIC->BindAction(LockOnAction, ETriggerEvent::Started, this, &ThisClass::Input_LockOn);
	}

	// 어빌리티: 누름/뗌을 InputTag와 함께 ASC로 전달
	for (const FODInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			EIC->BindAction(Action.InputAction, ETriggerEvent::Started, this, &ThisClass::Input_AbilityPressed, Action.InputTag);
			EIC->BindAction(Action.InputAction, ETriggerEvent::Completed, this, &ThisClass::Input_AbilityReleased, Action.InputTag);
		}
	}
}

void AODPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const AController* MyController = GetController();
	if (!MyController || IsDead())
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	// 쿼터뷰는 고정 카메라 방향, 3인칭은 컨트롤 방향 기준으로 움직인다.
	const FRotator YawRotation = IsValid(CombatModeComponent) ? CombatModeComponent->GetMovementBasisRotation() : FRotator(0.0f, MyController->GetControlRotation().Yaw, 0.0f);
	const FRotationMatrix YawMatrix(YawRotation);

	// 이동이 막혀도 방향은 기억한다. (캔슬 회피 방향 등)
	const FVector WorldDirection = YawMatrix.GetUnitAxis(EAxis::X) * Axis.Y + YawMatrix.GetUnitAxis(EAxis::Y) * Axis.X;
	if (!WorldDirection.IsNearlyZero())
	{
		BufferedMoveDirection = WorldDirection.GetSafeNormal2D();
		BufferedMoveTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}

	// 공격·가드·휴식 중에는 이동 입력을 받지 않는다.
	// 공격 모션은 제자리 모션(루트 이동 없음)이라 이동 입력을 받으면 미끄러져 보인다. 전진은 콤보 어빌리티의 StepDistance가 담당한다.
	// 행동 캔슬은 회피만 가능하다. (입력 방향은 위에서 기록해 두어 회피 방향에 쓰인다)
	if (IsValid(AbilitySystemComponent)
		&& (AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Defending) || AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Attacking)
			|| AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Resting)))
	{
		return;
	}

	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void AODPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	if (!GetController())
	{
		return;
	}

	// 쿼터뷰(고정 카메라)와 컷신 중에는 시점을 돌리지 않는다.
	if (IsValid(CombatModeComponent) && !CombatModeComponent->AllowsLookInput())
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();

	// 락온 중에는 카메라가 대상을 따라가므로 시점을 돌리지 않고, 크게 휘두르면 대상을 바꾼다.
	if (IsValid(LockOnComponent) && LockOnComponent->IsLockedOn())
	{
		LockOnComponent->HandleLookInput(Axis);
		return;
	}

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AODPlayerCharacter::Input_AbilityPressed(FGameplayTag InputTag)
{
	// 회피 키는 짧게/길게 누르기를 구분해야 하므로 바로 넘기지 않는다.
	if (InputTag.MatchesTagExact(ODGameplayTags::InputTag_Dodge))
	{
		Input_DodgePressed();
		return;
	}

	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void AODPlayerCharacter::Input_AbilityReleased(FGameplayTag InputTag)
{
	if (InputTag.MatchesTagExact(ODGameplayTags::InputTag_Dodge))
	{
		Input_DodgeReleased();
		return;
	}

	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->AbilityInputTagReleased(InputTag);
	}
}

void AODPlayerCharacter::Input_DodgePressed()
{
	bDodgeKeyHeld = true;
	bSprintStartedThisPress = false;

	// 누르는 순간 항상 회피한다. (캔슬 구간·방향 처리는 ASC의 회피 입력 처리와 같다)
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->AbilityInputTagPressed(ODGameplayTags::InputTag_Dodge);
		AbilitySystemComponent->AbilityInputTagReleased(ODGameplayTags::InputTag_Dodge);
	}

	// 그대로 누르고 있으면 회피가 끝나는 대로 달리기로 이어진다.
	GetWorldTimerManager().SetTimer(SprintHoldTimer, this, &ThisClass::TryStartSprintFromHold, SprintHoldTime, false);
}

void AODPlayerCharacter::Input_DodgeReleased()
{
	if (!bDodgeKeyHeld)
	{
		return;
	}

	bDodgeKeyHeld = false;
	GetWorldTimerManager().ClearTimer(SprintHoldTimer);

	// 달리는 중이면 끝낸다. (달리기는 키를 떼면 끝나는 어빌리티)
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->AbilityInputTagReleased(ODGameplayTags::InputTag_Sprint);
	}
}

void AODPlayerCharacter::TryStartSprintFromHold()
{
	if (!bDodgeKeyHeld || bSprintStartedThisPress || !IsValid(AbilitySystemComponent) || IsDead())
	{
		return;
	}

	if (AbilitySystemComponent->TryActivateAbilityByInputTag(ODGameplayTags::InputTag_Sprint))
	{
		bSprintStartedThisPress = true;
		return;
	}

	// 회피·공격 중이라 지금은 못 달린다. 키를 누르고 있으면 끝나는 대로 달리기 시작한다.
	GetWorldTimerManager().SetTimer(SprintHoldTimer, this, &ThisClass::TryStartSprintFromHold, SprintRetryInterval, false);
}

void AODPlayerCharacter::Input_Interact()
{
	if (IsValid(InteractionComponent) && !IsDead())
	{
		InteractionComponent->TryInteract();
	}
}

void AODPlayerCharacter::Input_LockOn()
{
	if (IsValid(LockOnComponent) && !IsDead())
	{
		LockOnComponent->ToggleLockOn();
	}
}

void AODPlayerCharacter::Revive()
{
	Super::Revive();

	// 사망(부활)·테스트 초기화 시 물약을 채운다.
	if (IsValid(PotionComponent))
	{
		PotionComponent->Refill();
	}
}

void AODPlayerCharacter::RestoreVitals()
{
	Super::RestoreVitals();

	if (IsValid(PotionComponent))
	{
		PotionComponent->Refill();
	}
}

AActor* AODPlayerCharacter::GetLockOnTarget() const
{
	return IsValid(LockOnComponent) ? LockOnComponent->GetTarget() : nullptr;
}

void AODPlayerCharacter::Input_Debug_SwapWeapon()
{
	if (TestWeapons.IsEmpty() || !IsValid(EquipmentComponent) || IsDead())
	{
		return;
	}

	// 공격·방어 중이면 교체하지 않는다.
	if (IsValid(AbilitySystemComponent) && (AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Attacking) || AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Defending)))
	{
		return;
	}

	const int32 CurrentIndex = TestWeapons.IndexOfByKey(EquipmentComponent->GetWeaponData());
	const int32 NextIndex = (CurrentIndex + 1) % TestWeapons.Num();
	EquipmentComponent->EquipWeapon(TestWeapons[NextIndex]);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("[테스트] 무기: %s"), *GetNameSafe(TestWeapons[NextIndex])));
	}
}

void AODPlayerCharacter::Input_Debug_Reset()
{
	// OD 게임 모드면 부활과 같은 경로로 초기화한다. (예약된 부활 취소 → 플레이어 회복 → 보스전·입구·적 초기화)
	if (AODGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AODGameMode>() : nullptr)
	{
		GameMode->CancelPendingRespawn();
		ResetForTest();
		GameMode->ResetWorldState();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[테스트] 모든 캐릭터 상태 초기화"));
		}
		return;
	}

	for (TActorIterator<AODCharacterBase> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			It->ResetForTest();
		}
	}

	// 보스방 입구도 처음 상태로 (들어간 플레이어는 문 앞으로 돌아가고 필드 모드가 된다)
	for (TActorIterator<AODBossEntrance> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			It->ResetForTest();
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[테스트] 모든 캐릭터 상태 초기화"));
	}
}

FVector AODPlayerCharacter::GetDesiredMoveDirection() const
{
	const UWorld* World = GetWorld();
	if (World && BufferedMoveTime >= 0.0 && World->GetTimeSeconds() - BufferedMoveTime <= 0.2)
	{
		return BufferedMoveDirection;
	}
	return Super::GetDesiredMoveDirection();
}
