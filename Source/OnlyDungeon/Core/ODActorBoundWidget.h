// OnlyDungeon 액터에 묶이는 위젯 인터페이스

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ODActorBoundWidget.generated.h"

class AActor;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UODActorBoundWidget : public UInterface
{
	GENERATED_BODY()
};

/**
 * 특정 액터의 상태를 보여 주는 위젯 (머리 위 이름표 등).
 * 게임플레이 코드가 UI 헤더에 의존하지 않도록(CLAUDE.md 12-1) 위젯에 액터를 넘길 때는 이 인터페이스만 쓴다.
 * 위젯은 넘겨받은 액터를 약한 참조로 들고, 필요한 이벤트(Attribute 변경 등)를 스스로 구독·해제한다.
 */
class ONLYDUNGEON_API IODActorBoundWidget
{
	GENERATED_BODY()

public:
	/** 표시할 액터를 정한다. nullptr이면 연결을 끊는다. */
	virtual void BindToActor(AActor* InActor) = 0;
};
