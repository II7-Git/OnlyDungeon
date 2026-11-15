// OnlyDungeon 적 캐릭터 기반 (잡몹·보스 공통)

#pragma once

#include "CoreMinimal.h"
#include "Character/ODCharacterBase.h"
#include "ODEnemyCharacter.generated.h"

class AODEnemyCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FODEnemyCombatActiveSignature, AODEnemyCharacter*, Enemy, bool, bActive);

/**
 * 잡몹과 보스가 공통으로 상속하는 적 캐릭터.
 * - 전투 활성 상태: bStartDormant면 대기 상태로 시작해 ActivateCombat()이 불릴 때까지 싸우지 않는다. (보스는 플레이어 입장 후 활성화)
 * AI(StateTree) 연결과 보스 페이즈 처리는 이후 단계에서 추가한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API AODEnemyCharacter : public AODCharacterBase
{
	GENERATED_BODY()

public:
	AODEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 전투를 시작한다. (대기 중이던 보스가 플레이어 입장 후 움직이기 시작) */
	UFUNCTION(BlueprintCallable, Category = "OD|Enemy")
	void ActivateCombat();

	/** 전투를 멈추고 대기 상태로 돌아간다. */
	UFUNCTION(BlueprintCallable, Category = "OD|Enemy")
	void DeactivateCombat();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Enemy")
	bool IsCombatActive() const { return bCombatActive; }

	/** 초기화 시 전투 활성 상태도 시작 상태(bStartDormant)로 되돌린다. */
	virtual void ResetForTest() override;

	/** 전투 활성 상태가 바뀔 때 */
	UPROPERTY(BlueprintAssignable, Category = "OD|Enemy")
	FODEnemyCombatActiveSignature OnCombatActiveChanged;

protected:
	virtual void BeginPlay() override;

	/** 전투 활성 상태가 바뀐 직후 (AI 시작·정지 등 파생 클래스 처리) */
	virtual void HandleCombatActiveChanged(bool bNewActive);

	/** 대기 상태로 시작한다. 보스처럼 누군가 ActivateCombat()을 불러 줄 때까지 싸우지 않는 적에 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Enemy")
	bool bStartDormant = false;

private:
	void SetCombatActive(bool bNewActive);

	bool bCombatActive = true;
};
