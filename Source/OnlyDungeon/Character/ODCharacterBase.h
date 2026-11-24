// OnlyDungeon 캐릭터 공통 기반 (플레이어·적)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "AbilitySystem/ODAbilitySet.h"
#include "ODCharacterBase.generated.h"

class UODAbilitySystemComponent;
class UODAttributeSet_Health;
class UODAttributeSet_Combat;
class UODAbilitySet;
class UODEquipmentComponent;
class UNiagaraSystem;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODOnCharacterDeathSignature, AODCharacterBase*, DeadCharacter);

/**
 * 플레이어와 적이 공유하는 캐릭터 기반.
 * - ASC와 AttributeSet(체력·전투)을 직접 보유한다. (싱글플레이: ASC는 캐릭터에 둔다)
 * - 사망해도 액터를 파괴하지 않는다. 부활은 위치 이동 + 상태 초기화로 처리한다. (부활 로직은 이후 단계)
 */
UCLASS(Abstract)
class ONLYDUNGEON_API AODCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface
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

	/** 조작자가 원하는 이동 방향(월드). 기본은 직전 이동 입력. 플레이어는 이동이 막힌 동안의 입력도 기억한다. */
	virtual FVector GetDesiredMoveDirection() const { return GetLastMovementInputVector(); }

	/** 현재 락온 대상. 락온 시스템 구현 전까지는 nullptr를 반환한다. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Character")
	virtual AActor* GetLockOnTarget() const { return nullptr; }

	/** 장비(무기) 관리 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Equipment")
	UODEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	/** 화면에 표시할 이름 (비어 있으면 클래스 이름) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Character")
	FText GetDisplayName() const;

	UNiagaraSystem* GetHitImpactEffect() const { return HitImpactEffect; }
	float GetHitImpactScale() const { return HitImpactScale; }
	UNiagaraSystem* GetHitGroundEffect() const { return HitGroundEffect; }
	float GetHitGroundScale() const { return HitGroundScale; }
	USoundBase* GetHitImpactSound() const { return HitImpactSound; }

	/**
	 * 타격 정지(히트스톱): Duration(초) 동안 이 캐릭터만 거의 멈췄다가 되돌린다. 이미 정지 중이면 더 긴 쪽으로 늘린다.
	 * 월드 시간이 아니라 이 액터의 시간 배율만 바꾸므로 다른 캐릭터·카메라는 그대로 움직인다.
	 */
	void ApplyHitStop(float Duration);

	/** 이 캐릭터가 맞을 때 공격자와 함께 멈추는 시간(초). 0이면 정지하지 않는다. (큰 적은 길게) */
	float GetHitStopDuration() const { return HitStopDuration; }

	//~ IGenericTeamAgentInterface
	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(TeamId); }
	//~ End

	/** 다른 진영인지 (무기 판정·범위 판정·락온이 적대 대상만 고른다). OD 캐릭터가 아니면 적대로 본다. */
	bool IsHostileTo(const AActor* Other) const;

	/**
	 * 부활(상태 초기화): 진행 중 행동·사망 연출 취소, 사망 해제, 기본 어빌리티 세트 재부여(초기 수치 GE·패시브 재발동),
	 * 현재 무기 재장착, 이동·충돌 복구. 위치는 바꾸지 않는다. (옮기는 건 호출한 쪽: 게임 모드, 적의 ResetToSpawn)
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Character")
	virtual void Revive();

	/** 휴식 회복: 체력·스태미나 등 초기 수치만 되돌린다. Revive와 달리 진행 중인 어빌리티·몽타주를 끊지 않는다. */
	virtual void RestoreVitals();

	/** [테스트용] 상태 초기화. 테스트 맵의 초기화 키(F5)에서 사용한다. (= Revive) */
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

	/** 진영 (0 = 플레이어, 1 = 적). 같은 진영끼리는 서로 맞지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Character")
	uint8 TeamId = 1;

	/** 화면에 표시할 이름 (HUD의 대상 체력 바 등) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Character")
	FText DisplayName;

	/** 근접 공격에 맞았을 때 맞은 지점에서 재생할 이펙트 (사람·해골은 피, 골렘은 불꽃 스파크). GameplayCue.Hit가 재생한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Character|Hit")
	TObjectPtr<UNiagaraSystem> HitImpactEffect;

	/** 피격 이펙트 크기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Character|Hit", meta = (ClampMin = "0.01"))
	float HitImpactScale = 1.0f;

	/** 맞을 때 발밑 바닥에 남는 이펙트 (피 웅덩이 등, 한 번 재생 후 스스로 사라지는 에셋). 공격 반대 방향으로 튄다. 비우면 없음 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Character|Hit")
	TObjectPtr<UNiagaraSystem> HitGroundEffect;

	/** 바닥 이펙트 크기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Character|Hit", meta = (ClampMin = "0.01", EditCondition = "HitGroundEffect != nullptr"))
	float HitGroundScale = 1.0f;

	/**
	 * 근접 공격에 맞았을 때 맞은 지점에서 낼 소리 (구울은 살점, 골렘은 돌·금속 등 몸 재질에 맞춘다). GameplayCue.Hit가 재생한다.
	 * 여러 소리를 번갈아 내려면 SoundCue(Random 노드)를 지정하고, 거리 감쇠·동시 재생 제한은 사운드 에셋 쪽에서 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Character|Hit")
	TObjectPtr<USoundBase> HitImpactSound;

	/** 맞을 때 공격자와 이 캐릭터가 함께 멈추는 시간(초). 타격감을 위한 짧은 정지다. 0이면 끔 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Character|Hit", meta = (ClampMin = "0.0", ClampMax = "0.3", Units = "s"))
	float HitStopDuration = 0.06f;

	/** 시작할 때 부여할 기본 어빌리티·이펙트 세트 (초기 수치 GE 포함) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Ability")
	TObjectPtr<UODAbilitySet> DefaultAbilitySet;

	/** 기본 세트로 부여한 핸들 */
	UPROPERTY()
	FODAbilitySet_GrantedHandles DefaultAbilitySetHandles;

private:
	bool bAbilitySystemInitialized = false;

	/** 사망 시 끄기 전의 캡슐 Pawn 채널 응답 (부활 때 복구) */
	TEnumAsByte<ECollisionResponse> SavedPawnResponse = ECR_Block;
	bool bDeathCollisionApplied = false;

private:
	/** 히트스톱이 끝나면 시간 배율을 되돌린다. */
	void EndHitStop();

	FTimerHandle HitStopTimer;
	double HitStopEndTime = 0.0;
};
