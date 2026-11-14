// OnlyDungeon 게임 모드

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Character/ODCombatModeComponent.h"
#include "ODGameMode.generated.h"

/**
 * OnlyDungeon 기본 게임 모드. (BP_ODGameMode의 부모)
 * 레벨이 어떤 전투 모드로 시작하는지 정한다. 레벨별로 다르게 하려면 월드 세팅에서 이 클래스를 상속한 게임 모드를 지정한다.
 */
UCLASS()
class ONLYDUNGEON_API AODGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	EODCombatMode GetInitialCombatMode() const { return InitialCombatMode; }

protected:
	/** 레벨 시작 시 플레이어의 전투 모드 (필드 = 쿼터뷰, 보스 = 3인칭) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Mode")
	EODCombatMode InitialCombatMode = EODCombatMode::Field;
};
