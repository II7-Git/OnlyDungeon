// OnlyDungeon 전투 모드 관리 컴포넌트

#include "ODCombatModeComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"
#include "AbilitySystem/ODAbilitySystemComponent.h"
#include "Character/ODPlayerCharacter.h"
#include "Core/ODGameMode.h"
#include "Core/ODGameplayTags.h"
#include "Core/ODLogChannels.h"

UODCombatModeComponent::UODCombatModeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	// 필드: 위에서 내려다보는 쿼터뷰 (시점 고정, 벽 충돌로 당기지 않음)
	FieldSettings.ArmLength = 1400.0f;
	FieldSettings.bUseControlRotation = false;
	FieldSettings.FixedArmRotation = FRotator(-55.0f, 45.0f, 0.0f);
	FieldSettings.bDoCollisionTest = false;
	FieldSettings.CameraLagSpeed = 10.0f;
	FieldSettings.FieldOfView = 90.0f;
	FieldSettings.bShowMouseCursor = true;

	// 보스: 캐릭터 뒤 3인칭
	BossSettings.ArmLength = 400.0f;
	BossSettings.bUseControlRotation = true;
	BossSettings.EnterPitch = -15.0f;
	BossSettings.bDoCollisionTest = true;
	BossSettings.CameraLagSpeed = 0.0f;
	BossSettings.FieldOfView = 90.0f;
}

void UODCombatModeComponent::BeginPlay()
{
	Super::BeginPlay();

	EODCombatMode InitialMode = DefaultMode;
	if (const UWorld* World = GetWorld())
	{
		if (const AODGameMode* GameMode = World->GetAuthGameMode<AODGameMode>())
		{
			InitialMode = GameMode->GetInitialCombatMode();
		}
	}
	SetCombatMode(InitialMode, true);
}

const FODCombatModeSettings& UODCombatModeComponent::GetSettings(EODCombatMode Mode) const
{
	return Mode == EODCombatMode::Field ? FieldSettings : BossSettings;
}

void UODCombatModeComponent::SetCombatMode(EODCombatMode NewMode, bool bForce)
{
	if (bModeApplied && NewMode == CurrentMode && !bForce)
	{
		return;
	}

	const bool bChanged = !bModeApplied || NewMode != CurrentMode;
	CurrentMode = NewMode;
	bModeApplied = true;

	ApplyCamera(GetSettings(NewMode), true);
	ApplyInputMapping(NewMode);
	ApplyModeTags(NewMode);
	ApplyCursor(GetSettings(NewMode));

	UE_LOG(LogOD, Log, TEXT("%s 전투 모드: %s"), *GetNameSafe(GetOwner()), NewMode == EODCombatMode::Field ? TEXT("필드(쿼터뷰)") : TEXT("보스(3인칭)"));
	if (bChanged)
	{
		OnCombatModeChanged.Broadcast(NewMode);
	}
}

void UODCombatModeComponent::ReapplyCurrentMode()
{
	if (!bModeApplied)
	{
		return;
	}
	// 카메라 각도는 그대로 두고(진입 처리 없이) 입력 매핑과 태그만 다시 맞춘다.
	ApplyCamera(GetSettings(CurrentMode), false);
	ApplyInputMapping(CurrentMode);
	ApplyModeTags(CurrentMode);
	ApplyCursor(GetSettings(CurrentMode));
}

void UODCombatModeComponent::ApplyCamera(const FODCombatModeSettings& Settings, bool bEnteringMode)
{
	const AODPlayerCharacter* Player = Cast<AODPlayerCharacter>(GetOwner());
	if (!IsValid(Player))
	{
		return;
	}

	if (USpringArmComponent* Boom = Player->GetCameraBoom(); IsValid(Boom))
	{
		Boom->TargetArmLength = Settings.ArmLength;
		Boom->bDoCollisionTest = Settings.bDoCollisionTest;
		Boom->bEnableCameraLag = Settings.CameraLagSpeed > 0.0f;
		Boom->CameraLagSpeed = Settings.CameraLagSpeed;
		Boom->bUsePawnControlRotation = Settings.bUseControlRotation;
		Boom->SetUsingAbsoluteRotation(!Settings.bUseControlRotation);
		if (!Settings.bUseControlRotation)
		{
			Boom->SetWorldRotation(Settings.FixedArmRotation);
		}
	}

	if (UCameraComponent* Camera = Player->GetFollowCamera(); IsValid(Camera))
	{
		Camera->SetFieldOfView(Settings.FieldOfView);
	}

	// 3인칭으로 들어올 때는 캐릭터 뒤에서 시작한다.
	if (bEnteringMode && Settings.bUseControlRotation)
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			PC->SetControlRotation(FRotator(Settings.EnterPitch, Player->GetActorRotation().Yaw, 0.0f));
		}
	}
}

void UODCombatModeComponent::ApplyInputMapping(EODCombatMode Mode)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		// 아직 빙의 전. 컨트롤러가 붙으면 ReapplyCurrentMode에서 다시 적용된다.
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		return;
	}

	const FODCombatModeSettings& Wanted = GetSettings(Mode);
	const FODCombatModeSettings& Other = GetSettings(Mode == EODCombatMode::Field ? EODCombatMode::Boss : EODCombatMode::Field);

	if (IsValid(Other.InputMapping) && Other.InputMapping != Wanted.InputMapping && Subsystem->HasMappingContext(Other.InputMapping))
	{
		Subsystem->RemoveMappingContext(Other.InputMapping);
	}
	if (IsValid(Wanted.InputMapping) && !Subsystem->HasMappingContext(Wanted.InputMapping))
	{
		Subsystem->AddMappingContext(Wanted.InputMapping, Wanted.InputPriority);
	}
}

void UODCombatModeComponent::ApplyModeTags(EODCombatMode Mode)
{
	const AODCharacterBase* Character = Cast<AODCharacterBase>(GetOwner());
	UODAbilitySystemComponent* ASC = IsValid(Character) ? Character->GetODAbilitySystemComponent() : nullptr;
	if (!IsValid(ASC))
	{
		return;
	}
	ASC->SetLooseGameplayTagCount(ODGameplayTags::Mode_Field, Mode == EODCombatMode::Field ? 1 : 0);
	ASC->SetLooseGameplayTagCount(ODGameplayTags::Mode_Boss, Mode == EODCombatMode::Boss ? 1 : 0);
}

void UODCombatModeComponent::ApplyCursor(const FODCombatModeSettings& Settings)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	PC->SetShowMouseCursor(Settings.bShowMouseCursor);
	if (Settings.bShowMouseCursor)
	{
		// 커서를 화면에 둔 채 클릭도 게임 입력으로 받는다. (클릭 중에도 커서를 숨기지 않아야 조준 방향이 유지된다)
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

bool UODCombatModeComponent::GetCursorAimDirection(FVector& OutDirection) const
{
	if (!GetSettings(CurrentMode).bShowMouseCursor || bInCinematic)
	{
		return false;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const APlayerController* PC = IsValid(Character) ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC)
	{
		return false;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}

	// 캐릭터 발 높이의 수평면과 커서 광선의 교점
	const FVector CharacterLocation = Character->GetActorLocation();
	const float HalfHeight = IsValid(Character->GetCapsuleComponent()) ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	const double Distance = (CharacterLocation.Z - HalfHeight - RayOrigin.Z) / RayDirection.Z;
	if (Distance <= 0.0)
	{
		return false;
	}

	FVector ToCursor = RayOrigin + RayDirection * Distance - CharacterLocation;
	ToCursor.Z = 0.0;
	if (ToCursor.IsNearlyZero())
	{
		return false;
	}

	OutDirection = ToCursor.GetSafeNormal2D();
	return true;
}

void UODCombatModeComponent::SetCinematicMode(bool bEnable)
{
	if (bInCinematic == bEnable)
	{
		return;
	}
	bInCinematic = bEnable;

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	if (bEnable)
	{
		// 진행 중인 행동을 끊고 눌린 입력을 정리한 뒤 입력을 막는다.
		if (const AODCharacterBase* Character = Cast<AODCharacterBase>(Pawn))
		{
			if (UODAbilitySystemComponent* ASC = Character->GetODAbilitySystemComponent(); IsValid(ASC))
			{
				ASC->CancelActionAbilities();
				ASC->ClearAbilityInput();
			}
		}
		if (PC && IsValid(Pawn))
		{
			Pawn->DisableInput(PC);
		}
	}
	else if (PC && IsValid(Pawn))
	{
		Pawn->EnableInput(PC);
	}

	OnCinematicChanged.Broadcast(bEnable);
}

FRotator UODCombatModeComponent::GetMovementBasisRotation() const
{
	const FODCombatModeSettings& Settings = GetSettings(CurrentMode);
	if (!Settings.bUseControlRotation)
	{
		return FRotator(0.0f, Settings.FixedArmRotation.Yaw, 0.0f);
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = IsValid(Pawn) ? Pawn->GetController() : nullptr;
	return FRotator(0.0f, Controller ? Controller->GetControlRotation().Yaw : 0.0f, 0.0f);
}

bool UODCombatModeComponent::AllowsLookInput() const
{
	return GetSettings(CurrentMode).bUseControlRotation && !bInCinematic;
}
