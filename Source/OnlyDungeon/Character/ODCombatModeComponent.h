// OnlyDungeon 전투 모드 관리 컴포넌트 (필드 쿼터뷰 ↔ 보스 3인칭)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ODCombatModeComponent.generated.h"

class UInputMappingContext;

/** 전투 모드 */
UENUM(BlueprintType)
enum class EODCombatMode : uint8
{
	/** 필드: 쿼터뷰 핵앤슬래시 */
	Field,
	/** 보스방: 3인칭 소울라이크 */
	Boss,
};

/** 모드 하나의 카메라·입력 설정 */
USTRUCT(BlueprintType)
struct FODCombatModeSettings
{
	GENERATED_BODY()

	/** 카메라 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode", meta = (ClampMin = "0.0", Units = "cm"))
	float ArmLength = 400.0f;

	/** true: 마우스로 시점을 돌린다(3인칭). false: FixedArmRotation으로 고정(쿼터뷰) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode")
	bool bUseControlRotation = true;

	/** 고정 카메라 각도 (bUseControlRotation이 false일 때). 이동 기준 방향도 이 Yaw를 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode", meta = (EditCondition = "!bUseControlRotation"))
	FRotator FixedArmRotation = FRotator(-55.0f, 45.0f, 0.0f);

	/** 모드로 들어올 때 카메라 피치 (bUseControlRotation일 때, 캐릭터 뒤에서 이 각도로 시작) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode", meta = (EditCondition = "bUseControlRotation"))
	float EnterPitch = -15.0f;

	/**
	 * 카메라 암 끝(카메라가 도는 중심)의 높이 보정 (월드 공간).
	 * 3인칭에서 카메라를 캐릭터 약간 위에 두어 내려다보게 한다. (소울류 3인칭 시점)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode", meta = (Units = "cm"))
	FVector CameraTargetOffset = FVector::ZeroVector;

	/** 카메라가 벽에 막히면 당겨 올지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode")
	bool bDoCollisionTest = true;

	/** 카메라 지연 따라가기 (0이면 끔) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode", meta = (ClampMin = "0.0"))
	float CameraLagSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float FieldOfView = 90.0f;

	/** 마우스 커서를 보여 주고 커서 방향으로 조준하는지 (쿼터뷰) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode")
	bool bShowMouseCursor = false;

	/** 이 모드의 입력 매핑 (모드가 바뀌면 교체) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode")
	TObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Mode")
	int32 InputPriority = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODCombatModeChangedSignature, EODCombatMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODCinematicChangedSignature, bool, bInCinematic);

/**
 * 플레이어의 전투 모드를 관리한다. (기획서 6-3 "모드 전환 담당")
 * 모드가 바뀌면 카메라(거리·각도·시점 회전), 입력 매핑, 모드 태그(Mode.Field / Mode.Boss), 이동 기준 방향을 한꺼번에 바꾼다.
 * 컷신 중에는 플레이어 입력을 막고 진행 중인 행동을 끊는다. (HUD는 OnCinematicChanged를 보고 숨는다)
 * 시작 모드는 AODGameMode::InitialCombatMode, OD 게임 모드가 아니면 DefaultMode.
 * 필드 모드에서는 마우스 커서를 보여 주고, 공격이 커서 방향으로 나가도록 조준 방향을 제공한다. (락온은 UODLockOnComponent)
 */
UCLASS(ClassGroup = (OnlyDungeon), meta = (BlueprintSpawnableComponent))
class ONLYDUNGEON_API UODCombatModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UODCombatModeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Mode")
	EODCombatMode GetCombatMode() const { return CurrentMode; }

	/** 모드를 바꾼다. 같은 모드여도 bForce면 다시 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "OD|Mode")
	void SetCombatMode(EODCombatMode NewMode, bool bForce = false);

	/** 현재 모드를 다시 적용한다. (컨트롤러가 붙은 직후 입력 매핑 등) */
	void ReapplyCurrentMode();

	/** 컷신 상태: 입력 차단, 진행 중 행동 취소, HUD 숨김 알림 */
	UFUNCTION(BlueprintCallable, Category = "OD|Mode")
	void SetCinematicMode(bool bEnable);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Mode")
	bool IsInCinematic() const { return bInCinematic; }

	/** 이동 입력의 기준 회전 (쿼터뷰는 고정 카메라 Yaw, 3인칭은 컨트롤 Yaw) */
	FRotator GetMovementBasisRotation() const;

	/** 마우스 시점 입력을 받을지 */
	bool AllowsLookInput() const;

	/**
	 * 캐릭터에서 마우스 커서가 가리키는 바닥 지점으로의 방향 (Z 제거·정규화).
	 * 커서를 쓰는 모드가 아니거나 구할 수 없으면 false.
	 * 충돌 판정 대신 캐릭터 발 높이의 수평면과 교차시켜, 지형·적에 가려도 방향이 튀지 않게 한다.
	 */
	bool GetCursorAimDirection(FVector& OutDirection) const;

	const FODCombatModeSettings& GetSettings(EODCombatMode Mode) const;

	UPROPERTY(BlueprintAssignable, Category = "OD|Mode")
	FODCombatModeChangedSignature OnCombatModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "OD|Mode")
	FODCinematicChangedSignature OnCinematicChanged;

protected:
	virtual void BeginPlay() override;

	/** 필드(쿼터뷰) 설정 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Mode")
	FODCombatModeSettings FieldSettings;

	/** 보스(3인칭) 설정 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Mode")
	FODCombatModeSettings BossSettings;

	/** 게임 모드가 AODGameMode가 아닐 때의 시작 모드 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Mode")
	EODCombatMode DefaultMode = EODCombatMode::Boss;

private:
	void ApplyCamera(const FODCombatModeSettings& Settings, bool bEnteringMode);
	void ApplyInputMapping(EODCombatMode Mode);
	void ApplyModeTags(EODCombatMode Mode);
	void ApplyCursor(const FODCombatModeSettings& Settings);

	EODCombatMode CurrentMode = EODCombatMode::Boss;
	bool bModeApplied = false;
	bool bInCinematic = false;
};
