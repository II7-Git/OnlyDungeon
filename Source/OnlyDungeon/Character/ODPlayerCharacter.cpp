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
}

void AODPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// 기본 입력 매핑 등록. 템플릿 PlayerController가 이미 등록한 매핑과 중복되어도 안전하다.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !IsValid(DefaultMappingContext))
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
	const FRotator YawRotation(0.0f, MyController->GetControlRotation().Yaw, 0.0f);
	const FRotationMatrix YawMatrix(YawRotation);

	// 이동이 막혀도 방향은 기억한다. (캔슬 회피 방향 등)
	const FVector WorldDirection = YawMatrix.GetUnitAxis(EAxis::X) * Axis.Y + YawMatrix.GetUnitAxis(EAxis::Y) * Axis.X;
	if (!WorldDirection.IsNearlyZero())
	{
		BufferedMoveDirection = WorldDirection.GetSafeNormal2D();
		BufferedMoveTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}

	// 공격·가드 중에는 이동 입력을 받지 않는다.
	// 공격 모션은 제자리 모션(루트 이동 없음)이라 이동 입력을 받으면 미끄러져 보인다. 전진은 콤보 어빌리티의 StepDistance가 담당한다.
	// 단, 캔슬 구간(후딜)에서는 행동을 끊고 바로 움직인다.
	if (IsValid(AbilitySystemComponent)
		&& (AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Defending) || AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Attacking)))
	{
		if (!AbilitySystemComponent->HasMatchingGameplayTag(ODGameplayTags::State_Cancelable))
		{
			return;
		}
		AbilitySystemComponent->CancelActionAbilities();
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

	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AODPlayerCharacter::Input_AbilityPressed(FGameplayTag InputTag)
{
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void AODPlayerCharacter::Input_AbilityReleased(FGameplayTag InputTag)
{
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->AbilityInputTagReleased(InputTag);
	}
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
	for (TActorIterator<AODCharacterBase> It(GetWorld()); It; ++It)
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
