// OnlyDungeon 상호작용 대상 인터페이스

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ODInteractable.generated.h"

class APawn;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UODInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 플레이어가 상호작용 키로 사용할 수 있는 대상 (문, 레버, 아이템 등).
 * 대상은 자기 범위(트리거)에 플레이어가 들어오고 나갈 때 UODInteractionComponent에 등록·해제한다.
 */
class ONLYDUNGEON_API IODInteractable
{
	GENERATED_BODY()

public:
	/** 지금 상호작용할 수 있는지 (이미 사용함, 진행 중 등이면 false) */
	virtual bool CanInteract(const APawn* InstigatorPawn) const { return true; }

	/** 상호작용 실행 */
	virtual void Interact(APawn* InstigatorPawn) = 0;

	/** HUD 안내 문구 (예: "보스방 입장") */
	virtual FText GetInteractionText() const { return FText::GetEmpty(); }
};
