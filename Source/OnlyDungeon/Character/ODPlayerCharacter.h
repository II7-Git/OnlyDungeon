// OnlyDungeon 플레이어 캐릭터

#pragma once

#include "CoreMinimal.h"
#include "Character/ODCharacterBase.h"
#include "GameplayTagContainer.h"
#include "ODPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UODInputConfig;
class UODWeaponData;
class UODCombatModeComponent;
class UODInteractionComponent;
struct FInputActionValue;

/**
 * 플레이어 캐릭터.
 * - 이동/시점은 C++ 함수로, 그 외 행동은 InputTag → ASC로 전달해 어빌리티가 처리한다.
 * - 카메라·입력 매핑·이동 기준 방향은 전투 모드 컴포넌트(필드 쿼터뷰 / 보스 3인칭)가 정한다.
 * - 상호작용 키는 상호작용 컴포넌트의 포커스 대상(문 등)에 전달한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API AODPlayerCharacter : public AODCharacterBase
{
	GENERATED_BODY()

public:
	AODPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	UODCombatModeComponent* GetCombatModeComponent() const { return CombatModeComponent; }
	UODInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

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

	/** [테스트] 무기 교체 (TestWeapons를 순서대로) */
	void Input_Debug_SwapWeapon();

	/** [테스트] 월드의 모든 OD 캐릭터 상태 초기화 */
	void Input_Debug_Reset();

	/** 카메라 붐 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 카메라 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 전투 모드 (필드 쿼터뷰 ↔ 보스 3인칭) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Mode")
	TObjectPtr<UODCombatModeComponent> CombatModeComponent;

	/** 상호작용 대상 관리 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Interaction")
	TObjectPtr<UODInteractionComponent> InteractionComponent;

	/** 입력 액션 ↔ InputTag 매핑 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Input")
	TObjectPtr<UODInputConfig> InputConfig;

	/** 기본 입력 매핑 (전투 모드 컴포넌트에 모드별 매핑이 없을 때 사용) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 기본 입력 매핑 우선순위 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Input")
	int32 DefaultMappingPriority = 0;

	/** 최근 이동 입력 방향(월드)과 시각. 이동이 막힌 동안에도 기록한다. */
	FVector BufferedMoveDirection = FVector::ZeroVector;
	double BufferedMoveTime = -1.0;

	/** [테스트] 무기 교체 키로 순환할 무기 목록 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Test")
	TArray<TObjectPtr<UODWeaponData>> TestWeapons;
};
