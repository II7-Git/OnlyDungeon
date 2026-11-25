// OnlyDungeon 보스방 출구 문 (보스 처치 후 다음 지역으로)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ODInteractable.h"
#include "ODBossGate.generated.h"

class AODBossEntrance;
class UBoxComponent;
class UPrimitiveComponent;
class USoundBase;
class UStaticMeshComponent;
struct FHitResult;

/**
 * 보스방 안쪽의 다음 지역 문. 연결된 보스방 입구(LinkedEntrance)의 보스를 모두 처치하기 전에는 닫혀 있고 상호작용도 뜨지 않는다.
 * 처치한 뒤 문 앞에서 상호작용하면 두 문짝이 경첩을 축으로 OpenDuration 동안 열리고, 문을 막던 충돌(Blocker)이 꺼진다.
 * - 열림 상태는 저장하지 않는다. 다시 불러오거나 휴식·부활로 월드가 초기화되면 닫히지만, 이미 처치했으므로 바로 다시 열 수 있다.
 * - 평소 Tick하지 않는다. 여는 동안만 짧은 반복 타이머로 문짝 각도를 바꾼다.
 * 문짝·문틀 메시와 위치는 BP(BP_ODBossGate)에서 지정한다. 문짝은 각 경첩(LeftHinge/RightHinge)에 붙어 함께 돈다.
 */
UCLASS()
class ONLYDUNGEON_API AODBossGate : public AActor, public IODInteractable
{
	GENERATED_BODY()

public:
	AODBossGate(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// IODInteractable
	virtual bool CanInteract(const APawn* InstigatorPawn) const override;
	virtual void Interact(APawn* InstigatorPawn) override;
	virtual FText GetInteractionText() const override { return InteractionText; }

	/** 문을 닫힌 처음 상태로 되돌린다. (휴식·부활·테스트 초기화) */
	UFUNCTION(BlueprintCallable, Category = "OD|BossGate")
	void ResetForTest();

	/** 연결된 보스방을 클리어했는지 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|BossGate")
	bool IsUnlocked() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|BossGate")
	bool IsOpen() const { return bOpening || bOpened; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 왼쪽 문짝 경첩 (회전축) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<USceneComponent> LeftHinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<UStaticMeshComponent> LeftDoor;

	/** 오른쪽 문짝 경첩 (회전축) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<USceneComponent> RightHinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<UStaticMeshComponent> RightDoor;

	/** 문틀 (선택, BP에서 메시 지정) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<UStaticMeshComponent> Frame;

	/** 닫혀 있는 동안 지나가지 못하게 막는 충돌 (문짝 메시의 충돌과 무관하게 확실히 막는다) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<UBoxComponent> Blocker;

	/** 상호작용 범위 (문 앞, 방 쪽) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<UBoxComponent> InteractionTrigger;

	/** 이 입구의 보스를 모두 처치하면 열 수 있다. (레벨에 배치한 문마다 지정) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "OD|BossGate")
	TObjectPtr<AODBossEntrance> LinkedEntrance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossGate")
	FText InteractionText;

	/** 문 전체 폭. 경첩·충돌·상호작용 범위가 이 크기에 맞춰 놓인다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossGate|Door", meta = (ClampMin = "100.0", Units = "cm"))
	float DoorWidth = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossGate|Door", meta = (ClampMin = "100.0", Units = "cm"))
	float DoorHeight = 600.0f;

	/** 열렸을 때 문짝 각도. 양수면 문 앞(+X, 방 쪽)에서 볼 때 바깥(-X)으로 열린다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossGate|Door", meta = (ClampMin = "-170.0", ClampMax = "170.0", Units = "deg"))
	float OpenAngle = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossGate|Door", meta = (ClampMin = "0.1", Units = "s"))
	float OpenDuration = 1.5f;

	/** 상호작용 범위가 문 앞(방 쪽)으로 나와 있는 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossGate|Door", meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionDepth = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossGate|Audio")
	TObjectPtr<USoundBase> OpenSound;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 여는 연출 한 단계 */
	void UpdateOpen();

	/** 문짝 열림 정도 (0 닫힘 ~ 1 열림) */
	void SetOpenAlpha(float Alpha);

	FTimerHandle OpenTimer;
	double OpenStartTime = 0.0;
	bool bOpening = false;
	bool bOpened = false;
};
