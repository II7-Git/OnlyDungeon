// OnlyDungeon 피격 이펙트 GameplayCue

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "ODGameplayCueNotify_Hit.generated.h"

/**
 * 근접 공격에 맞았을 때의 이펙트 (GameplayCue.Hit). 피해 GE(GE_Damage_Melee)가 실행될 때 맞은 캐릭터에서 실행된다.
 * - 어떤 이펙트를 낼지는 맞은 캐릭터가 정한다(AODCharacterBase::HitImpactEffect). 사람·해골은 피, 골렘은 불꽃 스파크처럼 재질에 맞게.
 * - 위치·방향은 피해 GE 컨텍스트의 적중 정보(무기 판정 HitResult)를 쓴다. 이펙트는 공격자 반대 방향(맞아서 튀는 방향)을 향한다.
 * - 패링 중에 받은 공격은 피해가 없으므로 내지 않는다.
 * 이 클래스를 부모로 하는 BP(GC_Hit, 태그 GameplayCue.Hit)를 큐 경로(/Game)에 두어야 GAS가 찾는다.
 * 이후 피격 효과음·화면 흔들림도 여기에 더한다.
 */
UCLASS(Blueprintable)
class ONLYDUNGEON_API UODGameplayCueNotify_Hit : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
