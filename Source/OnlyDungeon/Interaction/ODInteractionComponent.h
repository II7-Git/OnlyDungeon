// OnlyDungeon 상호작용 컴포넌트 (플레이어)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ODInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FODInteractionFocusChangedSignature, AActor*, NewFocus);

/**
 * 플레이어가 지금 상호작용할 수 있는 대상을 관리한다.
 * 대상(IODInteractable)이 자기 트리거에 플레이어가 들어오고 나갈 때 Register/Unregister를 호출하고,
 * 이 컴포넌트는 그중 가장 가까운 사용 가능 대상을 포커스로 정해 알린다. (HUD 안내 문구)
 * 이벤트 기반이라 Tick을 쓰지 않는다.
 */
UCLASS(ClassGroup = (OnlyDungeon), meta = (BlueprintSpawnableComponent))
class ONLYDUNGEON_API UODInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UODInteractionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 상호작용 범위에 들어온 대상 등록 (IODInteractable 구현 액터) */
	void RegisterCandidate(AActor* Candidate);

	/** 범위에서 나간 대상 해제 */
	void UnregisterCandidate(AActor* Candidate);

	/** 포커스를 다시 계산한다. (대상의 사용 가능 상태가 바뀌었을 때) */
	void RefreshFocus();

	/** 포커스 대상과 상호작용한다. 성공하면 true */
	UFUNCTION(BlueprintCallable, Category = "OD|Interaction")
	bool TryInteract();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/** 포커스 대상의 안내 문구 (없으면 빈 문자열) */
	FText GetFocusedInteractionText() const;

	UPROPERTY(BlueprintAssignable, Category = "OD|Interaction")
	FODInteractionFocusChangedSignature OnFocusChanged;

private:
	TArray<TWeakObjectPtr<AActor>> Candidates;
	TWeakObjectPtr<AActor> FocusedActor;
};
