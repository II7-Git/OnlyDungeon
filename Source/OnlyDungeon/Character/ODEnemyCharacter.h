// OnlyDungeon 적 캐릭터 기반 (잡몹·보스 공통)

#pragma once

#include "CoreMinimal.h"
#include "Character/ODCharacterBase.h"
#include "ODEnemyCharacter.generated.h"

/**
 * 잡몹과 보스가 공통으로 상속하는 적 캐릭터.
 * AI(StateTree) 연결과 보스 페이즈 처리는 이후 단계에서 추가한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API AODEnemyCharacter : public AODCharacterBase
{
	GENERATED_BODY()

public:
	AODEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
