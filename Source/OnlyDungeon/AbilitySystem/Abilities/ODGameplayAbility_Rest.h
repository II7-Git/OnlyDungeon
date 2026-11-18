// OnlyDungeon 휴식 어빌리티 (세이브 포인트)

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ODGameplayAbility.h"
#include "ODGameplayAbility_Rest.generated.h"

class AActor;
class AODRespawnPoint;
class UAnimMontage;

/**
 * 세이브 포인트 휴식: 게임플레이 이벤트(Event.Rest, OptionalObject = 세이브 포인트)로 발동한다.
 * 불 쪽을 보고 천천히 무릎을 꿇었다가 일어나는 몽타주를 재생하고, 앉은 뒤 RestApplyTime에 회복·적 초기화·부활 위치 등록을 한다.
 * 휴식 중에는 이동·행동이 막히고 피해를 받지 않으며(State.Resting, State.Invulnerable) 무기를 숨긴다.
 * 몽타주와 시간은 BP(GA_Rest)에서 정한다.
 */
UCLASS(Abstract)
class ONLYDUNGEON_API UODGameplayAbility_Rest : public UODGameplayAbility
{
	GENERATED_BODY()

public:
	UODGameplayAbility_Rest(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 앉았다 일어나는 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Rest")
	TObjectPtr<UAnimMontage> RestMontage;

	UPROPERTY(EditDefaultsOnly, Category = "OD|Rest", meta = (ClampMin = "0.05"))
	float MontagePlayRate = 1.0f;

	/** 휴식 시작 후 회복이 적용되는 시각 (무릎을 꿇은 뒤로 맞춘다) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Rest", meta = (ClampMin = "0.0", Units = "s"))
	float RestApplyTime = 1.8f;

private:
	void ApplyRest();

	UFUNCTION()
	void OnMontageFinished();

	FTimerHandle ApplyTimer;
	TWeakObjectPtr<AODRespawnPoint> RestPoint;
	TWeakObjectPtr<AActor> HiddenWeapon;
	bool bRestApplied = false;
};
