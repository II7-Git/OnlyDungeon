// OnlyDungeon 화면 흔들림

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "ODCameraShakes.generated.h"

/**
 * 묵직한 화면 흔들림 (보스 포효, 페이즈 전환, 큰 적 쓰러짐).
 * 세기는 재생할 때 Scale로 조절한다. (UGameplayStatics::PlayWorldCameraShake로 거리 감쇠와 함께 재생)
 */
UCLASS()
class ONLYDUNGEON_API UODCameraShake_Heavy : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UODCameraShake_Heavy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * 짧고 가벼운 타격 흔들림 (플레이어가 때리거나 맞을 때). 세기는 재생할 때 Scale로 조절한다.
 * 위치보다 회전 위주로 짧게 흔들어 조작감을 해치지 않는다.
 */
UCLASS()
class ONLYDUNGEON_API UODCameraShake_Hit : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UODCameraShake_Hit(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
