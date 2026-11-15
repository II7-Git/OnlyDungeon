// OnlyDungeon 필드 서브시스템 (필드 전용 규칙: 적 등록·초기화)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ODFieldSubsystem.generated.h"

class AODEnemyCharacter;

/**
 * 월드의 적(잡몹·보스)을 등록해 두고 한꺼번에 초기화한다.
 * (게임 흐름 분담: 공통 규칙 = AODGameMode, 보스전 = UODBossEncounterSubsystem, 필드 = 이 클래스)
 * - 적은 BeginPlay/EndPlay에서 스스로 등록·해제한다. 액터 전체를 순회하지 않아도 된다.
 * - 부활·휴식 때 게임 모드가 ResetAllEnemies를 불러 모든 적을 시작 위치·체력으로 되돌린다. (기획서 2장)
 * 이후 스포너·드랍 같은 필드 전용 규칙은 여기에 추가한다.
 */
UCLASS()
class ONLYDUNGEON_API UODFieldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterEnemy(AODEnemyCharacter* Enemy);
	void UnregisterEnemy(AODEnemyCharacter* Enemy);

	/** 등록된 모든 적을 시작 위치로 되돌리고 부활시킨다. */
	UFUNCTION(BlueprintCallable, Category = "OD|Field")
	void ResetAllEnemies();

	/** 등록된 적 (무효 항목 제외) */
	UFUNCTION(BlueprintCallable, Category = "OD|Field")
	TArray<AODEnemyCharacter*> GetEnemies() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<AODEnemyCharacter>> Enemies;
};
