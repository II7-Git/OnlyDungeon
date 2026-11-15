// OnlyDungeon 적 AI 컨트롤러 (StateTree)

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ODAIController.generated.h"

class UStateTreeAIComponent;
class AODEnemyCharacter;

/**
 * 적(잡몹·보스)이 쓰는 AI 컨트롤러. StateTree로 행동을 결정한다. (기획서 6-3: 보스 AI = StateTree)
 * - StateTree 에셋은 적 캐릭터(AODEnemyCharacter::StateTree)가 정한다. 같은 컨트롤러로 잡몹·보스를 모두 돌린다.
 * - 로직은 적이 살아 있고 전투 활성 상태일 때만 돈다. 대기(보스 입장 전)·사망 중에는 멈추고, 부활하면 처음부터 다시 시작한다.
 *   적 캐릭터가 상태가 바뀔 때 RefreshLogic을 부른다.
 */
UCLASS()
class ONLYDUNGEON_API AODAIController : public AAIController
{
	GENERATED_BODY()

public:
	AODAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 적의 상태(생존·전투 활성)에 맞춰 StateTree를 시작하거나 멈춘다.
	 * @param bRestart 이미 돌고 있어도 처음 상태부터 다시 시작한다. (부활)
	 */
	void RefreshLogic(bool bRestart = false);

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

private:
	/** 멈출 때 이동·시선 고정도 푼다. */
	void StopLogicAndMovement(const TCHAR* Reason);
};
