// OnlyDungeon 애님 노티파이 스테이트: 무기 판정 구간

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ODAnimNotifyState_WeaponTrace.generated.h"

class AODWeaponActor;

/**
 * 공격 모션의 판정 구간. 소유 액터의 장비 컴포넌트가 들고 있는 무기의 궤적 판정을 켠다.
 * 구간 동안 매 프레임 무기가 스윕하고, 맞은 대상은 Event.Melee.Hit 이벤트로 공격 어빌리티에 전달된다.
 */
UCLASS(meta = (DisplayName = "OD Weapon Trace"))
class ONLYDUNGEON_API UODAnimNotifyState_WeaponTrace : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UODAnimNotifyState_WeaponTrace();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	static AODWeaponActor* FindWeapon(const USkeletalMeshComponent* MeshComp);
};
