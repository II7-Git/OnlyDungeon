// OnlyDungeon 리스폰 포인트 (휴식 지점)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ODInteractable.h"
#include "Character/ODCombatModeComponent.h"
#include "ODRespawnPoint.generated.h"

class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
struct FHitResult;

/**
 * 휴식 지점. 다크소울의 화톳불처럼 F(상호작용)로 휴식하면:
 *  - 이 지점이 부활 위치로 등록된다. (사망하면 여기서 부활)
 *  - 플레이어 체력·스태미나가 회복되고, 잡몹과 보스가 모두 초기화된다.
 * 실제 처리는 게임 모드(공통 규칙)가 한다. 이 액터는 상호작용과 부활 위치만 제공한다.
 * 표식 메시는 BP(BP_ODRespawnPoint)에서 지정한다.
 */
UCLASS()
class ONLYDUNGEON_API AODRespawnPoint : public AActor, public IODInteractable
{
	GENERATED_BODY()

public:
	AODRespawnPoint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// IODInteractable
	virtual bool CanInteract(const APawn* InstigatorPawn) const override;
	virtual void Interact(APawn* InstigatorPawn) override;
	virtual FText GetInteractionText() const override { return InteractionText; }

	/** 부활할 위치·방향 (바닥 높이) */
	FTransform GetRespawnTransform() const;

	/** 여기서 부활할 때의 전투 모드 (보스방 앞 = 필드) */
	EODCombatMode GetRespawnCombatMode() const { return RespawnCombatMode; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|RespawnPoint")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 표식 (BP에서 메시 지정) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|RespawnPoint")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	/** 상호작용 범위 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|RespawnPoint")
	TObjectPtr<UBoxComponent> InteractionTrigger;

	/** 부활 위치 (바닥 높이, 화살표 방향을 바라본다) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|RespawnPoint")
	TObjectPtr<UArrowComponent> SpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|RespawnPoint")
	FText InteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|RespawnPoint")
	EODCombatMode RespawnCombatMode = EODCombatMode::Field;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
