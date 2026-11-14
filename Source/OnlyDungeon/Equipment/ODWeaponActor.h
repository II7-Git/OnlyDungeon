// OnlyDungeon 무기 액터

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ODWeaponActor.generated.h"

class UODWeaponData;
class UStaticMeshComponent;

/**
 * 캐릭터 손에 붙는 무기. 메시 표시와 무기 궤적 판정을 담당한다.
 * - 틱하지 않는다. 판정은 애님 노티파이(UODAnimNotifyState_WeaponTrace)가 구간 동안 TickTrace를 호출한다.
 * - 칼날을 따라 여러 지점을 이전 프레임 위치에서 현재 위치까지 스윕해 빠른 휘두르기도 놓치지 않는다.
 * - 한 구간에서 같은 대상은 한 번만 맞는다. 맞으면 소유자 ASC에 Event.Melee.Hit 이벤트를 보낸다.
 * - 필드 모드에서는 스윕 대신 PerformAreaHit으로 전방 부채꼴 안의 대상을 한 번에 맞힌다. (결과는 같은 Event.Melee.Hit)
 * - 디버그 표시: 콘솔 OD.Debug.WeaponTrace 1
 */
UCLASS()
class ONLYDUNGEON_API AODWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	AODWeaponActor();

	/** 무기 데이터로 메시와 판정 설정을 적용한다. */
	virtual void InitializeFromData(const UODWeaponData* InWeaponData);

	/** 판정 구간 시작 (맞은 대상 목록 초기화) */
	void BeginTrace();

	/** 판정 구간 중 매 프레임 호출 */
	void TickTrace();

	/** 판정 구간 끝 */
	void EndTrace();

	/** 필드 모드 범위 판정: 소유자 전방 부채꼴 안의 대상을 한 번씩 맞힌다. (판정 구간 시작 시 1회 호출) */
	void PerformAreaHit();

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

protected:
	/** 무기 메시 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Weapon")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

private:
	/** 칼날 판정 지점들의 현재 월드 위치 */
	void GetTracePoints(TArray<FVector>& OutPoints) const;

	/** 새 대상을 맞혔을 때 소유자에게 알린다. */
	void SendHitEvent(const FHitResult& Hit) const;

	FVector TraceStart = FVector(0.0, 0.0, -15.0);
	FVector TraceEnd = FVector(0.0, 0.0, -100.0);
	float TraceRadius = 8.0f;
	int32 TraceSamples = 4;
	float FieldHitRadius = 220.0f;
	float FieldHitHalfAngle = 60.0f;

	bool bTracing = false;
	TArray<FVector> PreviousPoints;
	TArray<TWeakObjectPtr<AActor>> HitActors;
};
