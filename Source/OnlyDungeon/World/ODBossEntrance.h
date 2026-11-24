// OnlyDungeon 보스방 입구 (문 + 입장 컷신)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ODInteractable.h"
#include "Character/ODCombatModeComponent.h"
#include "ODBossEntrance.generated.h"

class UArrowComponent;
class UMaterialInstanceDynamic;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class ACharacter;
class AODEnemyCharacter;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODBossEntranceSignature, APawn*, EnteredPawn);

/**
 * 보스방 입구. 플레이어가 문 앞 트리거 안에서 상호작용하면:
 *  1) 컷신 상태로 전환(입력 차단, HUD 숨김), 문 정면 지점(EntryPoint)까지 걸어가 문 쪽으로 돌아선다.
 *  2) 그 자리에서 입장 레벨 시퀀스가 시작되고 문이 열린다. 시퀀스는 EntryPoint 기준 상대 좌표로 재생된다. (Transform Origin)
 *     문 안쪽 통로의 안개가 드러났다가 걸어 들어가는 동안 걷힌다.
 *  3) 플레이어가 문을 지나 보스방 지점(RoomPoint)까지 걸어 들어간다.
 *     이동은 실제 이동 입력(AddMovementInput)으로 줘서 걷기·조깅 애니메이션이 그대로 재생된다.
 *     (Root Motion Source는 가속이 없어 애니메이션 블루프린트가 대기 상태로 남는다)
 *     이동 입력은 매 프레임 넣어야 해서 컷신 이동 중에만 Tick을 켠다. (평소에는 꺼져 있음)
 *  4) 걷는 동안(시퀀스 카메라가 화면을 잡고 있을 때) 전투 모드를 보스(3인칭)로 바꾸고,
 *     시퀀스가 끝나면 시퀀스 카메라에서 3인칭 카메라로 블렌드한다.
 *  5) 컷신 종료, 문이 닫힌다(안개문처럼 되돌아갈 수 없음). OnEntered 발생.
 *  6) Bosses가 지정되어 있으면 보스전 서브시스템에 보스전 시작을 요청한다. (보스 활성화, 보스 체력 바)
 *     보스를 모두 잡으면(승리) 문이 다시 열린다.
 * 시퀀스 카메라 경로는 EntryPoint→RoomPoint 거리(기본 950cm) 기준으로 만들어져 있다.
 */
UCLASS()
class ONLYDUNGEON_API AODBossEntrance : public AActor, public IODInteractable
{
	GENERATED_BODY()

public:
	AODBossEntrance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// IODInteractable
	virtual bool CanInteract(const APawn* InstigatorPawn) const override;
	virtual void Interact(APawn* InstigatorPawn) override;
	virtual FText GetInteractionText() const override { return InteractionText; }

	/**
	 * [테스트·부활] 입구를 처음 상태로 되돌리고, 마지막으로 들어간 플레이어를 문 앞으로 돌려보낸다.
	 * 보스를 처치한 입구(IsCleared)는 문을 연 채 두고 플레이어도 옮기지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Test")
	void ResetForTest();

	/** 지정한 보스를 모두 처치했는지 (보스전 서브시스템의 처치 기록). 처치한 입구는 초기화해도 열린 채로 남고 다시 들어갈 수 없다. */
	bool IsCleared() const;

	/** 입장 시작 지점 (문 앞 바닥, 문을 향함). 테스트 이동 등에서 쓴다. */
	FTransform GetEntryTransform() const;

	/** 입장이 끝났을 때 (보스 활성화 등) */
	UPROPERTY(BlueprintAssignable, Category = "OD|BossEntrance")
	FODBossEntranceSignature OnEntered;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 왼쪽 문짝 경첩 (이 컴포넌트가 회전한다) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<USceneComponent> LeftHinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<UStaticMeshComponent> LeftDoor;

	/** 오른쪽 문짝 경첩 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<USceneComponent> RightHinge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<UStaticMeshComponent> RightDoor;

	/** 문틀 (위쪽 상인방) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<UStaticMeshComponent> Lintel;

	/**
	 * 문 안쪽 통로의 안개. 문이 닫혀 있을 때는 문짝에 가려 보이지 않고, 문이 열리면 드러났다가
	 * 걸어 들어가는 동안 서서히 걷힌다. 입구가 초기화되면 다시 나타난다. (메시·재질은 BP에서 지정, 재질의 Fade 파라미터로 옅어진다)
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<UStaticMeshComponent> FogWall;

	/** 상호작용 범위 (문 앞) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<UBoxComponent> InteractionTrigger;

	/** 입장 시작 지점 (문 앞, 바닥 높이, 문을 향함). 시퀀스의 원점 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<UArrowComponent> EntryPoint;

	/** 보스방 안 도착 지점 (바닥 높이) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<UArrowComponent> RoomPoint;

	/** 이 문 안의 보스. 입장이 끝나면 전투를 시작한다. (레벨에 배치한 입구마다 지정, 보스는 bStartDormant를 켠다) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "OD|BossEntrance")
	TArray<TObjectPtr<AODEnemyCharacter>> Bosses;

	/** 보스를 모두 잡으면 문을 다시 연다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	bool bOpenOnVictory = true;

	/** 입장 컷신 (EntryPoint 기준 상대 좌표로 재생) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	TObjectPtr<ULevelSequence> EntrySequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	FText InteractionText;

	/** 입장 후 전투 모드 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance")
	EODCombatMode ModeAfterEntry = EODCombatMode::Boss;

	/** 문 전체 폭 (두 문짝 합). 문짝·상인방·안개·상호작용 범위가 이 크기에 맞춰 배치된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Door", meta = (ClampMin = "100.0", Units = "cm"))
	float DoorWidth = 300.0f;

	/** 문짝 높이 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Door", meta = (ClampMin = "100.0", Units = "cm"))
	float DoorHeight = 400.0f;

	/** 문짝 두께 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Door", meta = (ClampMin = "1.0", Units = "cm"))
	float DoorThickness = 20.0f;

	/** 문 위 상인방 높이 (문짝 위를 덮어 틈을 막는다) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Door", meta = (ClampMin = "1.0", Units = "cm"))
	float LintelHeight = 100.0f;

	/** 안개가 문짝 뒤(방 안쪽)로 들어가 있는 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Fog", meta = (ClampMin = "0.0", Units = "cm"))
	float FogInset = 120.0f;

	/** 시퀀스 시작 후 안개가 걷히기 시작하는 시각 (문이 열려 안개가 보인 뒤) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Fog", meta = (ClampMin = "0.0", Units = "s"))
	float FogFadeDelay = 1.0f;

	/** 안개가 다 걷히는 데 걸리는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Fog", meta = (ClampMin = "0.05", Units = "s"))
	float FogFadeTime = 1.5f;

	/** 문 열림 각도 / 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Door")
	float DoorOpenAngle = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Door", meta = (ClampMin = "0.05", Units = "s"))
	float DoorOpenTime = 0.9f;

	/** 시퀀스 시작 후 문이 열리기 시작하는 시각 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Door", meta = (ClampMin = "0.0", Units = "s"))
	float DoorOpenDelay = 0.3f;

	/** 입장 후 문을 닫을지 (되돌아갈 수 없게) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Door")
	bool bCloseBehind = true;

	/** 걸어 들어가는 속도 (조깅 모션이 맞도록 플레이어 이동 속도와 맞춘다) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Walk", meta = (ClampMin = "50.0", Units = "cm/s"))
	float WalkSpeed = 500.0f;

	/** 시퀀스 시작 후 문 안으로 걸어 들어가기 시작하는 최소 시각 (문이 어느 정도 열린 뒤) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Walk", meta = (ClampMin = "0.0", Units = "s"))
	float WalkInStartTime = 1.2f;

	/** 문 쪽으로 돌아서는 속도 (도/초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Walk", meta = (ClampMin = "30.0"))
	float TurnRate = 360.0f;

	/** 목표 지점에 이만큼 가까워지면 도착으로 본다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance|Walk", meta = (ClampMin = "1.0", Units = "cm"))
	float ArriveTolerance = 15.0f;

	/** 시퀀스가 없을 때 3인칭 카메라로 블렌드하는 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|BossEntrance", meta = (ClampMin = "0.0", Units = "s"))
	float FallbackCameraBlendTime = 0.6f;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnSequenceFinished();

	UFUNCTION()
	void HandleEncounterEnded(bool bVictory);

	/** 문 앞 도착·회전 후: 시퀀스 재생, 문 열기, 걸어 들어가기 예약 */
	void StartEntrySequence();
	void StartWalkIn();
	void FinishEntry();

	/** 컷신 중 캐릭터 이동 단계 */
	enum class EWalkPhase : uint8 { None, Approach, Turn, WalkIn };

	/** 컷신 이동 갱신 (이동 중에만 켜지는 Tick에서 호출) */
	void UpdateWalk(float DeltaSeconds);
	void StartWalkPhase(EWalkPhase Phase);
	void StopWalk();

	/** 문 열림 정도(0~1)를 목표값으로 시간에 걸쳐 바꾼다. */
	void AnimateDoor(float TargetAlpha);
	void UpdateDoor();
	void SetDoorAlpha(float Alpha);

	/** DoorWidth·DoorHeight에 맞춰 문짝·상인방·안개·상호작용 범위를 배치한다. */
	void ApplyDoorLayout();

	/** 안개 걷힘 (0 = 다 걷힘, 1 = 짙음) */
	void StartFogFade();
	void UpdateFogFade();
	void SetFogAmount(float Amount);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FogMaterial;

	double FogFadeStartTime = 0.0;
	FTimerHandle FogDelayTimer;
	FTimerHandle FogFadeTimer;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	TWeakObjectPtr<APawn> EnteringPawn;
	bool bUsed = false;

	/** 이 입구가 시작한 보스전이 진행 중인지 */
	bool bEncounterStarted = false;
	bool bInProgress = false;

	float DoorAlpha = 0.0f;
	float DoorAlphaFrom = 0.0f;
	float DoorAlphaTo = 0.0f;
	double DoorAnimStartTime = 0.0;

	FTimerHandle DoorTimer;
	FTimerHandle DoorDelayTimer;
	FTimerHandle WalkTimer;
	FTimerHandle FinishTimer;

	EWalkPhase WalkPhase = EWalkPhase::None;
	double WalkPhaseStartTime = 0.0;
	float WalkPhaseTimeout = 0.0f;
};
