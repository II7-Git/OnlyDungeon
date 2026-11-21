// OnlyDungeon 캐릭터 애님 인스턴스 공통 부모

#include "Character/ODAnimInstance.h"

namespace ODAnimInstance
{
	/** 애님 그래프 Pose Snapshot 노드(Named Snapshot 모드)가 읽는 스냅샷 이름 */
	static const FName SwapSnapshotName(TEXT("ODWeaponSwap"));
}

void UODAnimInstance::StartBlendFromSnapshot(const FPoseSnapshot& Snapshot, float Duration)
{
	if (!Snapshot.bIsValid || Duration <= 0.0f)
	{
		SwapBlendAlpha = 0.0f;
		return;
	}

	SwapSnapshot = Snapshot;
	// Pose Snapshot 노드의 Snapshot Pin 모드는 스크립트로 핀을 만들 수 없어 Named Snapshot 모드로 읽게 한다.
	AddPoseSnapshot(ODAnimInstance::SwapSnapshotName) = Snapshot;
	SwapBlendDuration = Duration;
	SwapBlendElapsed = 0.0f;
	SwapBlendAlpha = 1.0f;
}

void UODAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (SwapBlendAlpha <= 0.0f)
	{
		return;
	}

	SwapBlendElapsed += DeltaSeconds;
	// 부드럽게 빠지도록 감소량에 smoothstep을 적용한다.
	const float T = FMath::Clamp(SwapBlendElapsed / SwapBlendDuration, 0.0f, 1.0f);
	SwapBlendAlpha = 1.0f - FMath::SmoothStep(0.0f, 1.0f, T);
	if (T >= 1.0f)
	{
		SwapBlendAlpha = 0.0f;
		SwapSnapshot.bIsValid = false;
	}
}
