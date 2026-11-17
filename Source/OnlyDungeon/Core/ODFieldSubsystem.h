// OnlyDungeon 필드 서브시스템 (필드 전용 규칙: 적 등록·초기화)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ODFieldSubsystem.generated.h"

class AActor;
class AODCharacterBase;
class AODEnemyCharacter;

/**
 * 월드의 적(잡몹·보스)을 등록해 두고 한꺼번에 초기화한다.
 * (게임 흐름 분담: 공통 규칙 = AODGameMode, 보스전 = UODBossEncounterSubsystem, 필드 = 이 클래스)
 * - 적은 BeginPlay/EndPlay에서 스스로 등록·해제한다. 액터 전체를 순회하지 않아도 된다.
 * - 부활·휴식 때 게임 모드가 ResetAllEnemies를 불러 모든 적을 시작 위치·체력으로 되돌린다. (기획서 2장)
 * - 몬스터 무리 규칙: 동료 같이 어그로(AlertNearbyEnemies), 동시 공격 수 제한(공격 순번).
 *   소울라이크처럼 여럿이 한꺼번에 때리지 않고 1~2마리씩 들어오고, 나머지는 주위를 돌며 기다린다.
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

	/** Source 주변(Radius 안)의 다른 적에게 같은 대상을 알린다. (한 마리가 발견하면 무리가 함께 달려온다) */
	void AlertNearbyEnemies(AODEnemyCharacter* Source, AODCharacterBase* Target, float Radius);

	/** 공격 순번을 새로 얻을 수 있는지 (이미 가진 적은 참) */
	bool CanAcquireAttackToken(const AODEnemyCharacter* Enemy) const;

	/** 공격 순번을 얻는다. 동시에 공격하는 적이 MaxAttackTokens에 이미 찼으면 실패 */
	bool AcquireAttackToken(AODEnemyCharacter* Enemy);

	void ReleaseAttackToken(AODEnemyCharacter* Enemy);

	/** 플레이어를 동시에 공격할 수 있는 일반 몬스터 수 */
	int32 MaxAttackTokens = 2;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** 죽었거나 사라진 순번 보유자를 정리한다. */
	void PruneAttackTokens() const;

	TArray<TWeakObjectPtr<AODEnemyCharacter>> Enemies;

	/** 지금 공격 순번을 가진 적 */
	mutable TArray<TWeakObjectPtr<AODEnemyCharacter>> AttackTokenHolders;
};
