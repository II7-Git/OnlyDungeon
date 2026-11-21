// OnlyDungeon 캐릭터 애님 인스턴스 공통 부모

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/PoseSnapshot.h"
#include "ODAnimInstance.generated.h"

/**
 * 무기별 애님 블루프린트(ABP_OD_OneHand/TwoHand/Humanoid)의 공통 부모.
 * 무기 교체로 애님 인스턴스가 통째로 바뀌면 새 인스턴스는 이전 자세를 모르므로 자세가 한 프레임에 바뀐다.
 * 교체 직전 자세(스냅샷)를 받아 SwapBlendTime 동안 새 자세로 섞을 수 있게 값을 노출한다.
 * 애님 그래프는 출력 직전에 다중 블렌드(포즈0 = 원래 결과·알파 1-SwapBlendAlpha, 포즈1 = Pose Snapshot "ODWeaponSwap"·알파 SwapBlendAlpha, 정규화 끔)를 둔다.
 */
UCLASS()
class ONLYDUNGEON_API UODAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** 교체 직전 자세에서 이 인스턴스의 자세로 Duration초 동안 섞기 시작한다. Duration이 0 이하면 섞지 않는다. */
	void StartBlendFromSnapshot(const FPoseSnapshot& Snapshot, float Duration);

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** 교체 직전 자세 (애님 그래프의 Pose Snapshot 노드가 읽는다) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "OD|Anim")
	FPoseSnapshot SwapSnapshot;

	/** 스냅샷 자세의 비중. 1에서 시작해 0으로 줄어든다. 0이면 Blend 노드가 스냅샷을 평가하지 않는다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "OD|Anim")
	float SwapBlendAlpha = 0.0f;

private:
	float SwapBlendDuration = 0.0f;
	float SwapBlendElapsed = 0.0f;
};
