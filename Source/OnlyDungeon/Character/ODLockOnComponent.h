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
 * - 대상 전환: 락온 중 마우스를 좌우로 크게 휘두르면(짧은 시간 안에 누적 이동량이 임계값 이상) 그 방향에 있는 적으로 바꾼다.
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

	/**
	 * 락온 중 시점 입력을 받는다. (플레이어가 락온 중이면 카메라 회전 대신 이 함수로 넘긴다)
	 * 좌우 입력을 짧은 시간 누적해 크게 휘두르면 그 방향의 적으로 대상을 바꾼다.
	 */
	void HandleLookInput(const FVector2D& LookAxis);

	/**
	 * 현재 대상 기준 좌(-1)·우(+1) 방향에서 가장 가까운 적으로 대상을 바꾼다.
	 * @return 바꿀 대상이 있어 바꿨으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|LockOn")
	bool SwitchTarget(float Direction);

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

	/** 대상 전환: 이 시간 안에 누적한 좌우 시점 입력이 SwitchFlickThreshold 이상이면 전환 (마우스 픽셀 단위) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn|Switch", meta = (ClampMin = "1.0"))
	float SwitchFlickThreshold = 40.0f;

	/** 대상 전환: 좌우 입력을 누적하는 시간 창 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn|Switch", meta = (ClampMin = "0.05", Units = "s"))
	float SwitchFlickWindow = 0.25f;

	/** 대상 전환 후 다음 전환까지 기다리는 시간 (한 번 휘둘러 여러 명을 건너뛰지 않게) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn|Switch", meta = (ClampMin = "0.0", Units = "s"))
	float SwitchCooldown = 0.35f;

	/** 대상 전환: 현재 대상 방향에서 이 각도 안의 적만 후보로 본다 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn|Switch", meta = (ClampMin = "10.0", ClampMax = "180.0", Units = "deg"))
	float MaxSwitchAngle = 120.0f;

	/** 대상 중심에서 겨누는 높이 보정 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|LockOn", meta = (Units = "cm"))
	float TargetHeightOffset = 20.0f;

private:
	AActor* FindBestTarget() const;

	/** 락온할 수 있는 적 (반경 안, 살아 있음, 적대, 벽 뒤 아님). 카메라 정면 조건은 보지 않는다. */
	void GatherCandidates(TArray<AActor*>& OutCandidates) const;
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

	/** 대상 전환 판정용 좌우 시점 입력 누적값 */
	float FlickAccumulated = 0.0f;
	double FlickWindowStart = -1.0;
	double LastSwitchTime = -1.0;
};
