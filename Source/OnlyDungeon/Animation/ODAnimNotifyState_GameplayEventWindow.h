// OnlyDungeon 애님 노티파이 스테이트: 구간 시작·끝에 게임플레이 이벤트 전송

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "ODAnimNotifyState_GameplayEventWindow.generated.h"

/**
 * 애니메이션의 특정 구간을 어빌리티에 알리는 노티파이 스테이트.
 * 구간이 시작되면 EventTag 이벤트를 EventMagnitude = 1로, 끝나면 EventMagnitude = 0으로 소유 액터의 ASC에 보낸다.
 * 용도: 콤보 입력 구간(Event.Montage.ComboWindow), 이후 무기 판정 구간(Event.Montage.HitTrace) 등.
 * ASC가 없는 액터(애니메이션 에디터 미리보기 등)에서는 아무것도 하지 않는다.
 */
UCLASS(meta = (DisplayName = "OD Gameplay Event Window"))
class ONLYDUNGEON_API UODAnimNotifyState_GameplayEventWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 구간 시작·끝에 보낼 이벤트 태그 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD", meta = (Categories = "Event.Montage"))
	FGameplayTag EventTag;

private:
	void SendWindowEvent(const USkeletalMeshComponent* MeshComp, bool bOpen) const;
};
