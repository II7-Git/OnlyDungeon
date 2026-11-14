// OnlyDungeon 락온 컴포넌트 (보스 모드에서 대상 고정)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/ODCombatModeComponent.h"
#include "ODLockOnComponent.generated.h"

class AODCharacterBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODLockOnTargetChangedSignature, AActor*, NewTarget);

/**
 * 플레이어의 락온을 관리한다. (기획서 3장: 보스방 = 락온 대상 조준)
 * - 락온 키를 누르면 화면 중앙에 가장 가까운 적을 잡고, 다시 누르면 푼다.
 * - 락온 중에는 카메라(컨트롤 회전)가 대상을 따라가고, 캐릭터는 대상을 바라본 채 움직인다.
 * - State.LockOn 태그를 붙인다. 회피는 이 태그와 GetLockOnTarget()을 보고 8방향 구르기를 쓴다.
 * - 대상 사망, 거리 초과, 필드 모드 전환, 컷신 시작, 본인 사망 시 자동으로 풀린다.
 * 카메라가 매 프레임 대상을 따라가야 하므로 락온 중에만 Tick을 켠다.
 */
UCLASS(ClassGroup = (OnlyDungeon), meta = (BlueprintSpawnableComponent))
class ONLYDUNGEON_API UODLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UODLockOnComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 락온 중이면 풀고, 아니면 대상을 찾아 건다. */
	UFUNCTION(BlueprintCallable, Category = "OD|LockOn")
	void ToggleLockOn();

	/** 대상을 찾아 락온한다. 잡을 대상이 없으면 false. */
	UFUNCTION(BlueprintCallable, Category = "OD|LockOn")
	bool TryLockOn();

	UFUNCTION(BlueprintCallable, Category = "OD|LockOn")
	void ClearLockOn();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|LockOn")
	AActor* GetTarget() const { return Target.Get(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|LockOn")
	bool IsLockedOn() const { return Target.IsValid(); }

	/** 카메라와 표식이 겨누는 대상 위치 (대상 중심에서 TargetHeightOffset만큼 위) */
	FVector GetTargetAimLocation() const;

	/** 락온 대상이 바뀌거나 풀릴 때 (풀리면 nullptr) */
	UPROPERTY(BlueprintAssignable, Category = "OD|LockOn")
	FODLockOnTargetChangedSignature OnTargetChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 대상을 찾는 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn", meta = (ClampMin = "100.0", Units = "cm"))
	float SearchRadius = 2000.0f;

	/** 카메라 정면에서 이 각도 안에 있는 대상만 잡는다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn", meta = (ClampMin = "5.0", ClampMax = "180.0", Units = "deg"))
	float SearchHalfAngle = 50.0f;

	/** 대상과 이 거리보다 멀어지면 락온이 풀린다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn", meta = (ClampMin = "100.0", Units = "cm"))
	float BreakDistance = 2500.0f;

	/** 카메라가 대상을 따라가는 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn", meta = (ClampMin = "0.0"))
	float CameraInterpSpeed = 10.0f;

	/** 대상을 볼 때 더하는 카메라 피치 (음수면 살짝 내려다본다) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn", meta = (Units = "deg"))
	float CameraPitchOffset = -12.0f;

	/** 락온 중 카메라 피치 범위 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn")
	FVector2D CameraPitchRange = FVector2D(-45.0, 20.0);

	/** 대상 중심에서 겨누는 높이 보정 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn", meta = (Units = "cm"))
	float TargetHeightOffset = 20.0f;

private:
	AActor* FindBestTarget() const;
	void SetTarget(AActor* NewTarget);

	/** 락온 중에는 이동 방향 대신 대상(컨트롤 방향)을 바라보게 한다. */
	void ApplyCharacterFacing(bool bLockedOn) const;

	UFUNCTION()
	void HandleCharacterDeath(AODCharacterBase* DeadCharacter);

	UFUNCTION()
	void HandleCombatModeChanged(EODCombatMode NewMode);

	UFUNCTION()
	void HandleCinematicChanged(bool bInCinematic);

	TWeakObjectPtr<AActor> Target;
};
