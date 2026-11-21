// OnlyDungeon 회복 물약 (개수 관리)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ODPotionComponent.generated.h"

class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FODPotionChargesChangedSignature, int32, CurrentCharges, int32, MaxCharges);

/**
 * 플레이어의 회복 물약 개수를 관리한다. (기획서 5장: 횟수 제한 회복 아이템, 소울류 에스트 병 참고)
 * - 처음부터 MaxCharges개를 들고 시작하고, 휴식·사망(부활) 때 다시 가득 채운다. (AODPlayerCharacter::Revive)
 * - 실제 회복은 마시기 어빌리티(UODGameplayAbility_DrinkPotion)가 GE로 처리한다. 이 컴포넌트는 개수와 회복 비율만 가진다.
 * - HUD는 OnChargesChanged를 받아 남은 개수를 표시한다.
 * 이후 인벤토리가 생기면 아이템 시스템으로 옮길 수 있게 개수 관리만 따로 둔다.
 */
UCLASS(ClassGroup = (OnlyDungeon), meta = (BlueprintSpawnableComponent))
class ONLYDUNGEON_API UODPotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UODPotionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Potion")
	int32 GetCharges() const { return CurrentCharges; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Potion")
	int32 GetMaxCharges() const { return MaxCharges; }

	/** HUD 아이템 칸에 보일 물약 아이콘 (UI 재질). 비우면 이름 글자만 보인다. */
	const TSoftObjectPtr<UMaterialInterface>& GetHudIcon() const { return HudIcon; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Potion")
	bool HasCharges() const { return CurrentCharges > 0; }

	/** 물약 하나가 회복하는 양 (최대 체력 대비 비율) */
	float GetHealRatio() const { return HealRatio; }

	/** 물약 하나를 쓴다. 남은 게 없으면 false */
	bool TryConsume();

	/** 가득 채운다. (휴식, 부활) */
	UFUNCTION(BlueprintCallable, Category = "OD|Potion")
	void Refill();

	/** 개수가 바뀔 때 (HUD 표시) */
	UPROPERTY(BlueprintAssignable, Category = "OD|Potion")
	FODPotionChargesChangedSignature OnChargesChanged;

protected:
	virtual void BeginPlay() override;

	/** 최대 개수 (처음부터 들고 있는 개수) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Potion", meta = (ClampMin = "0"))
	int32 MaxCharges = 5;

	/** HUD 아이템 칸 아이콘 (Tools/UI/build_item_icons.py가 물약 병 메시를 찍어 만든다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Potion|UI")
	TSoftObjectPtr<UMaterialInterface> HudIcon;

	/** 하나당 회복량 (최대 체력 대비) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Potion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealRatio = 0.6f;

private:
	void SetCharges(int32 NewCharges);

	int32 CurrentCharges = 0;
};
