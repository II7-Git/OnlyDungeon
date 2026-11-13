// OnlyDungeon 캐릭터 공통 기반 (플레이어·적)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/ODAbilitySet.h"
#include "ODCharacterBase.generated.h"

class UODAbilitySystemComponent;
class UODAttributeSet_Health;
class UODAttributeSet_Combat;
class UODAbilitySet;
class UODEquipmentComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODOnCharacterDeathSignature, AODCharacterBase*, DeadCharacter);

/**
 * 플레이어와 적이 공유하는 캐릭터 기반.
 * - ASC와 AttributeSet(체력·전투)을 직접 보유한다. (싱글플레이: ASC는 캐릭터에 둔다)
 * - 사망해도 액터를 파괴하지 않는다. 부활은 위치 이동 + 상태 초기화로 처리한다. (부활 로직은 이후 단계)
 */
UCLASS(Abstract)
class ONLYDUNGEON_API AODCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AODCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface

	/** OnlyDungeon ASC */
	UFUNCTION(BlueprintCallable, Category = "OD|Character")
	UODAbilitySystemComponent* GetODAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** 사망 상태인지 (State.Dead 태그 기준) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Character")
	bool IsDead() const;

	/** 현재 락온 대상. 락온 시스템 구현 전까지는 nullptr를 반환한다. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Character")
	virtual AActor* GetLockOnTarget() const { return nullptr; }

	/** 장비(무기) 관리 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Equipment")
	UODEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	/**
	 * [테스트용] 상태 초기화: 사망 해제, 기본 어빌리티 세트 재부여(초기 수치 GE 포함), 현재 무기 재장착.
	 * 테스트 맵의 초기화 키에서 사용한다. 실제 부활 처리는 이후 단계에서 별도로 만든다.
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Test")
	virtual void ResetForTest();

	/** 사망했을 때 */
	UPROPERTY(BlueprintAssignable, Category = "OD|Character")
	FODOnCharacterDeathSignature OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** ASC 초기화와 기본 어빌리티 세트 부여. BeginPlay에서 한 번 호출된다. */
	virtual void InitializeAbilitySystem();

	/** 체력이 0이 되었을 때 */
	virtual void HandleOutOfHealth(AActor* DamageInstigator);

	/** 강인도가 0이 되었을 때 */
	virtual void HandlePoiseBroken(AActor* DamageInstigator);

	/** 어빌리티 시스템 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Ability")
	TObjectPtr<UODAbilitySystemComponent> AbilitySystemComponent;

	/** 체력·강인도 */
	UPROPERTY()
	TObjectPtr<UODAttributeSet_Health> HealthSet;

	/** 스태미나·공격력·방어력·무게 */
	UPROPERTY()
	TObjectPtr<UODAttributeSet_Combat> CombatSet;

	/** 장비(무기) 관리. ASC 초기화가 끝난 뒤 기본 무기를 장착한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OD|Equipment")
	TObjectPtr<UODEquipmentComponent> EquipmentComponent;

	/** 시작할 때 부여할 기본 어빌리티·이펙트 세트 (초기 수치 GE 포함) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Ability")
	TObjectPtr<UODAbilitySet> DefaultAbilitySet;

	/** 기본 세트로 부여한 핸들 */
	UPROPERTY()
	FODAbilitySet_GrantedHandles DefaultAbilitySetHandles;

private:
	bool bAbilitySystemInitialized = false;
};
