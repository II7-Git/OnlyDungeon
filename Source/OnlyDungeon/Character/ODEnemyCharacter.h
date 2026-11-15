// OnlyDungeon 적 캐릭터 기반 (잡몹·보스 공통)

#pragma once

#include "CoreMinimal.h"
#include "Character/ODCharacterBase.h"
#include "GameplayTagContainer.h"
#include "ODEnemyCharacter.generated.h"

class AODEnemyCharacter;
class UStateTree;
class UWidgetComponent;
struct FOnAttributeChangeData;

/** 보스 페이즈: 체력 비율이 HealthRatio 이하가 되면 PhaseTag를 붙인다. */
USTRUCT(BlueprintType)
struct FODEnemyPhase
{
	GENERATED_BODY()

	/** 이 비율(0~1) 이하의 체력에서 이 페이즈가 된다. 1페이즈는 1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Enemy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Enemy", meta = (Categories = "State.Phase"))
	FGameplayTag PhaseTag;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FODEnemyCombatActiveSignature, AODEnemyCharacter*, Enemy, bool, bActive);

/**
 * 잡몹과 보스가 공통으로 상속하는 적 캐릭터.
 * - 레벨 시작 위치를 기억했다가 플레이어 부활·휴식 때 그 자리로 돌아가 부활한다. (필드 서브시스템에 스스로 등록)
 * - 전투 활성 상태: bStartDormant면 대기 상태로 시작해 ActivateCombat()이 불릴 때까지 싸우지 않는다. (보스는 플레이어 입장 후 활성화)
 * - AI: AODAIController가 StateTree(이 클래스의 StateTree)를 돌린다. 공격은 플레이어와 같은 무기 어빌리티를 InputTag로 발동한다.
 *   공격 대상은 이 클래스가 관리한다(UpdateCombatTarget): 감지 범위 안의 플레이어, 시작 위치에서 너무 멀어지면 놓고 돌아간다.
 *   GetLockOnTarget()이 공격 대상을 돌려주므로 공격 어빌리티가 대상 쪽으로 나간다.
 * - 보스 페이즈: Phases에 따라 체력 비율로 State.Phase.* 태그를 바꾼다. StateTree가 이 태그로 패턴을 고른다.
 * - 적은 항상 Mode.Boss 태그를 가진다. (강공격·방어 어빌리티가 이 태그를 요구)
 * - 일반 몬스터는 머리 위 이름표(이름·체력)를 띄운다: 플레이어가 가까이 있거나, 플레이어와 싸우는 중이거나, 피해를 입은 상태일 때.
 *   보스는 bShowNameplate를 꺼서 이름표 대신 화면 하단 보스 체력 바만 쓴다.
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

	/** 부활 시 전투 활성 상태도 시작 상태(bStartDormant)로 되돌린다. */
	virtual void Revive() override;

	/** 레벨 시작 위치로 돌아가 부활한다. (플레이어 부활·휴식 시 필드 서브시스템이 호출) */
	UFUNCTION(BlueprintCallable, Category = "OD|Enemy")
	void ResetToSpawn();

	/** 테스트 초기화(F5)도 시작 위치로 돌아간다. */
	virtual void ResetForTest() override;

	/** 공격 대상 (AI가 고른 플레이어). 공격 어빌리티의 조준 방향이 된다. */
	virtual AActor* GetLockOnTarget() const override;

	/** 공격 대상을 갱신해 돌려준다. (감지 범위 진입 시 잡고, 시작 위치에서 LeashRange 이상 멀어지면 놓고 돌아간다) */
	AActor* UpdateCombatTarget();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Enemy")
	AActor* GetCombatTarget() const;

	FVector GetSpawnLocation() const { return SpawnTransform.GetLocation(); }

	UStateTree* GetStateTree() const { return StateTree; }

	/** 전투 활성 상태가 바뀔 때 */
	UPROPERTY(BlueprintAssignable, Category = "OD|Enemy")
	FODEnemyCombatActiveSignature OnCombatActiveChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleOutOfHealth(AActor* DamageInstigator) override;

	/** 전투 활성 상태가 바뀐 직후 (AI 시작·정지 등 파생 클래스 처리) */
	virtual void HandleCombatActiveChanged(bool bNewActive);

	/** 대기 상태로 시작한다. 보스처럼 누군가 ActivateCombat()을 불러 줄 때까지 싸우지 않는 적에 켠다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Enemy")
	bool bStartDormant = false;

	/** 행동 StateTree (잡몹 ST_OD_Minion, 보스 ST_OD_Boss). 비우면 AI가 돌지 않는다. (허수아비) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Enemy|AI")
	TObjectPtr<UStateTree> StateTree;

	/** 플레이어가 이 거리 안에 들어오면 공격 대상으로 잡는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Enemy|AI", meta = (ClampMin = "0.0", Units = "cm"))
	float AggroRange = 1200.0f;

	/** 시작 위치에서 이 거리 이상 멀어지면 대상을 놓고 돌아간다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Enemy|AI", meta = (ClampMin = "0.0", Units = "cm"))
	float LeashRange = 2500.0f;

	/** 보스 페이즈 (비우면 페이즈 없음). 체력 비율이 높은 순서로 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Enemy|Phase")
	TArray<FODEnemyPhase> Phases;

	/** 머리 위 이름표 (화면 공간 위젯) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Enemy|UI")
	TObjectPtr<UWidgetComponent> NameplateComponent;

	/** 머리 위 이름표를 쓰는지. 보스는 끄고 화면 하단 보스 체력 바를 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Enemy|UI")
	bool bShowNameplate = true;

	/** 플레이어가 이 거리 안에 있으면 이름표를 보여 준다. (전투 중이거나 피해를 입었으면 거리와 무관하게 보임) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Enemy|UI", meta = (ClampMin = "0.0", Units = "cm"))
	float NameplateShowDistance = 1500.0f;

	/** 캡슐 꼭대기에서 이름표까지의 높이 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Enemy|UI", meta = (Units = "cm"))
	float NameplateHeightOffset = 35.0f;

private:
	void SetCombatActive(bool bNewActive);

	/** 체력 비율에 맞는 페이즈 태그로 바꾼다. */
	void RefreshPhase();

	/** AI 컨트롤러에 상태 변화를 알린다. */
	void RefreshAI(bool bRestart) const;

	/** 이름표를 보일지 다시 판단한다. (주기 타이머, 사망·부활 시) */
	void RefreshNameplate();

	FTimerHandle NameplateTimer;

	TWeakObjectPtr<AODCharacterBase> CombatTarget;

	/** 대상을 놓고 시작 위치로 돌아가는 중 (도착할 때까지 새 대상을 잡지 않는다) */
	bool bReturningHome = false;

	FDelegateHandle HealthChangedHandle;

	bool bCombatActive = true;
	FTransform SpawnTransform = FTransform::Identity;
};
