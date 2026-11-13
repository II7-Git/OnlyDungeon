// OnlyDungeon 테스트용 허수아비

#pragma once

#include "CoreMinimal.h"
#include "Character/ODEnemyCharacter.h"
#include "GameplayTagContainer.h"
#include "ODTrainingDummy.generated.h"

/**
 * [테스트 전용] 전투 테스트 맵의 허수아비.
 * - AutoAttackInterval > 0이면 그 주기로 플레이어를 바라보고 AutoAttackInputTag 어빌리티를 발동한다. (패링·가드 테스트용)
 * - 플레이어가 AutoAttackRange 밖에 있거나, 사망·경직·공격 중이면 공격하지 않는다.
 * - 실제 적 AI는 StateTree로 별도 구현한다. 이 클래스는 게임 콘텐츠에 쓰지 않는다.
 */
UCLASS()
class ONLYDUNGEON_API AODTrainingDummy : public AODEnemyCharacter
{
	GENERATED_BODY()

public:
	AODTrainingDummy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 자동 공격 주기(초). 0이면 공격하지 않는다. 레벨에 배치한 개체마다 설정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Test", meta = (ClampMin = "0.0"))
	float AutoAttackInterval = 0.0f;

	/** 자동 공격에 사용할 입력 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Test", meta = (Categories = "InputTag"))
	FGameplayTag AutoAttackInputTag;

	/** 플레이어가 이 거리 안에 있을 때만 공격한다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Test", meta = (ClampMin = "0.0"))
	float AutoAttackRange = 350.0f;

private:
	void TryAutoAttack();

	FTimerHandle AutoAttackHandle;
};
