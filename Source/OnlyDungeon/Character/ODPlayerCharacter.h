// OnlyDungeon 플레이어 캐릭터

#pragma once

#include "CoreMinimal.h"
#include "Character/ODCharacterBase.h"
#include "GameplayTagContainer.h"
#include "ODPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UCameraShakeBase;
class UInputMappingContext;
class UODInputConfig;
class UODWeaponData;
class UODCombatModeComponent;
class UODInteractionComponent;
class UODLockOnComponent;
class UODPotionComponent;
struct FInputActionValue;

/**
 * 플레이어 캐릭터.
 * - 이동/시점은 C++ 함수로, 그 외 행동은 InputTag → ASC로 전달해 어빌리티가 처리한다.
 * - 카메라·입력 매핑·이동 기준 방향은 전투 모드 컴포넌트(필드 쿼터뷰 / 보스 3인칭)가 정한다.
 * - 상호작용 키는 상호작용 컴포넌트의 포커스 대상(문 등)에 전달한다.
 * - 락온 키는 락온 컴포넌트에 전달한다. 락온 중에는 마우스로 시점을 돌리지 않는다.
 * - 회피 키는 누르는 순간 항상 회피하고, 그대로 누르고 있으면 회피가 끝난 뒤 달리기(InputTag.Sprint)로 이어진다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API AODPlayerCharacter : public AODCharacterBase
{
	GENERATED_BODY()

public:
	AODPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** 타격 흔들림: bDealt = 플레이어가 때림(약하게), false = 맞음(조금 세게) */
	void PlayHitCameraShake(bool bDealt);
	UODCombatModeComponent* GetCombatModeComponent() const { return CombatModeComponent; }
	UODInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	UODLockOnComponent* GetLockOnComponent() const { return LockOnComponent; }
	UODPotionComponent* GetPotionComponent() const { return PotionComponent; }

	/** 부활: 기본 처리에 더해 회복 물약을 가득 채운다. */
	virtual void Revive() override;

	/** 휴식: 기본 회복에 더해 회복 물약을 가득 채운다. */
	virtual void RestoreVitals() override;

	/** 락온 컴포넌트가 잡고 있는 대상 (없으면 nullptr) */
	virtual AActor* GetLockOnTarget() const override;

	/** 이동이 막힌 동안(공격 중)에도 최근 0.2초 안에 누른 방향을 돌려준다. */
	virtual FVector GetDesiredMoveDirection() const override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_AbilityPressed(FGameplayTag InputTag);
	void Input_AbilityReleased(FGameplayTag InputTag);
	void Input_Interact();
	void Input_LockOn();

	/** 회피 키 누름: 바로 회피하고, 누르고 있으면 달리기로 이어지도록 타이머를 시작한다. */
	void Input_DodgePressed();

	/** 회피 키 뗌: 달리기를 끝내고 더 이상 달리기를 시도하지 않는다. */
	void Input_DodgeReleased();

	/** 회피 키를 누르고 있는 동안: 달리기를 시도한다. (회피·공격 중이라 실패하면 끝나는 대로 시작되도록 다시 시도) */
	void TryStartSprintFromHold();

	/** 무기 교체 (Tab, TestWeapons를 순서대로). 무기 교체 어빌리티가 있으면 넣었다 꺼내는 모션으로, 없으면 바로 바꾼다. */
	void Input_Debug_SwapWeapon();

	/** [테스트] 월드의 모든 OD 캐릭터 상태 초기화 */
	void Input_Debug_Reset();

	/** [테스트] 보스방 문 앞으로 이동 (AODGameMode::TeleportPlayerToBossDoor) */
	void Input_Debug_GoToBossDoor();

	/** 일시정지 메뉴 열기/닫기 (게임 모드에 요청) */
	void Input_Pause();

	/** 카메라 붐 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 카메라 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 때리거나 맞을 때 재생할 화면 흔들림 (기본: 짧은 타격 흔들림) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Feedback")
	TSubclassOf<UCameraShakeBase> HitCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Feedback", meta = (ClampMin = "0.0"))
	float DealtHitShakeScale = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Feedback", meta = (ClampMin = "0.0"))
	float TakenHitShakeScale = 0.8f;

	/** 체력 비율이 이 값 아래로 내려가면 화면 가장자리가 붉게 어두워진다. (낮을수록 진해짐) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthThreshold = 0.3f;

	/** 저체력 연출 최대 세기일 때의 비네트·색 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Feedback", meta = (ClampMin = "0.0"))
	float LowHealthVignette = 1.1f;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Feedback")
	FLinearColor LowHealthTint = FLinearColor(1.0f, 0.55f, 0.5f);

	/** 전투 모드 (필드 쿼터뷰 ↔ 보스 3인칭) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Mode")
	TObjectPtr<UODCombatModeComponent> CombatModeComponent;

	/** 상호작용 대상 관리 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Interaction")
	TObjectPtr<UODInteractionComponent> InteractionComponent;

	/** 락온 (보스 모드 대상 고정) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|LockOn")
	TObjectPtr<UODLockOnComponent> LockOnComponent;

	/** 회복 물약 개수 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Potion")
	TObjectPtr<UODPotionComponent> PotionComponent;

	/** 입력 액션 ↔ InputTag 매핑 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Input")
	TObjectPtr<UODInputConfig> InputConfig;

	/** 기본 입력 매핑 (전투 모드 컴포넌트에 모드별 매핑이 없을 때 사용) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 기본 입력 매핑 우선순위 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Input")
	int32 DefaultMappingPriority = 0;

	/** 회피 키를 누른 뒤 이 시간(초)부터 달리기를 시도한다. 회피 중에는 달리기가 막히므로 실제로는 회피가 끝난 뒤 시작된다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Input", meta = (ClampMin = "0.05", Units = "s"))
	float SprintHoldTime = 0.25f;

	/** 달리기를 못 시작했을 때 다시 시도하는 간격(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Input", meta = (ClampMin = "0.02", Units = "s"))
	float SprintRetryInterval = 0.1f;

	FTimerHandle SprintHoldTimer;

	/** 회피 키를 누르고 있는 중인지 */
	bool bDodgeKeyHeld = false;

	/** 이번 누름에서 달리기가 한 번 시작됐는지 (스태미나가 바닥나 멈추면 다시 눌러야 한다) */
	bool bSprintStartedThisPress = false;

	/** 최근 이동 입력 방향(월드)과 시각. 이동이 막힌 동안에도 기록한다. */
	FVector BufferedMoveDirection = FVector::ZeroVector;
	double BufferedMoveTime = -1.0;

	/** [테스트] 무기 교체 키로 순환할 무기 목록 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Test")
	TArray<TObjectPtr<UODWeaponData>> TestWeapons;

	virtual void InitializeAbilitySystem() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 체력 비율에 맞춰 저체력 연출(카메라 후처리 비네트·붉은 색)의 세기를 바꾼다. 체력이 바뀔 때만 호출된다. */
	void RefreshLowHealthEffect();

	FDelegateHandle LowHealthHandle;
};
