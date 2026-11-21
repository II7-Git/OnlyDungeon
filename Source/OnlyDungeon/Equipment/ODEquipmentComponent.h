// OnlyDungeon 장비 관리 컴포넌트

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "AbilitySystem/ODAbilitySet.h"
#include "ODEquipmentComponent.generated.h"

class AODWeaponActor;
class UODWeaponData;
class UStaticMeshComponent;

/**
 * 캐릭터의 장비를 관리한다. (현재는 오른손 무기 슬롯만. 다른 슬롯은 장비 단계에서 확장)
 * - 장착: 무기 액터 스폰·부착, (있으면) 왼손 장식 메시 부착, 어빌리티 세트 부여, 무기 타입 태그 추가, 이동 애니메이션 레이어 연결
 * - 해제: 위의 모든 것을 회수
 * - 무기별 애님 블루프린트는 다음 틱(몽타주 재생 중이면 모두 끝난 뒤)에 적용한다.
 * - 틱하지 않는다. 기본 무기는 소유 캐릭터가 ASC 초기화를 마친 뒤 EquipDefaultWeapon으로 장착한다.
 */
UCLASS(ClassGroup = (OD), meta = (BlueprintSpawnableComponent))
class ONLYDUNGEON_API UODEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UODEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 무기 장착 (이미 들고 있으면 먼저 해제) */
	UFUNCTION(BlueprintCallable, Category = "OD|Equipment")
	void EquipWeapon(UODWeaponData* WeaponData);

	/** 무기 해제 */
	UFUNCTION(BlueprintCallable, Category = "OD|Equipment")
	void UnequipWeapon();

	/**
	 * 공격·방어 어빌리티 실행 중 무기 부착 위치 전환.
	 * true면 무기 데이터의 AbilityAttachSocket으로, false면 기본 AttachSocket으로 되돌린다. (AbilityAttachSocket이 없으면 아무것도 하지 않음)
	 */
	UFUNCTION(BlueprintCallable, Category = "OD|Equipment")
	void SetAbilityAttachment(bool bAbilityAttach);

	/** DefaultWeapon 장착. 소유 캐릭터의 ASC가 준비된 뒤 호출한다. */
	void EquipDefaultWeapon();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Equipment")
	AODWeaponActor* GetWeaponActor() const { return WeaponActor; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "OD|Equipment")
	UODWeaponData* GetWeaponData() const { return WeaponData; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 시작할 때 장착할 무기 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OD|Equipment")
	TObjectPtr<UODWeaponData> DefaultWeapon;

	/** 무기별 애님 블루프린트를 바꿀 때 이전 자세에서 새 자세로 섞는 시간(초). 0이면 바로 바뀐다. 새 애님 인스턴스가 UODAnimInstance여야 적용된다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Equipment", meta = (ClampMin = "0.0"))
	float AnimSwapBlendTime = 0.25f;

private:
	UPROPERTY()
	TObjectPtr<AODWeaponActor> WeaponActor;

	/** 무기 데이터의 왼손 장식 메시 (방패 등) */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> OffhandMeshComponent;

	UPROPERTY()
	TObjectPtr<UODWeaponData> WeaponData;

	UPROPERTY()
	FODAbilitySet_GrantedHandles WeaponAbilityHandles;

	/** 무기 애님 블루프린트로 바꾸기 전의 기본 애님 블루프린트 */
	UPROPERTY()
	TSubclassOf<UAnimInstance> DefaultAnimInstanceClass;

	/**
	 * 애님 블루프린트 교체 요청. 바로 바꾸지 않고 다음 틱에 적용한다.
	 * 무기 교체는 몽타주 노티파이(애니메이션 갱신 중)에서 호출되는데, 엔진이 이때의 SetAnimInstanceClass를 거부하기 때문이다.
	 * 연달아 요청하면 마지막 요청만 적용한다.
	 */
	void RequestAnimClass(TSubclassOf<UAnimInstance> NewClass);

	/** 대기 중인 애님 블루프린트를 적용한다. 몽타주가 재생 중이면 교체가 몽타주를 끊으므로 모든 몽타주가 끝날 때까지 미룬다. */
	void TryApplyPendingAnimClass();

	/** 몽타주를 기다리던 애님 인스턴스에서 모든 몽타주가 끝남. 갱신 중일 수 있어 다음 틱으로 다시 미룬다. */
	UFUNCTION()
	void HandleAllMontagesEnded();

	/** 몽타주 종료 대기 바인딩 해제 */
	void UnbindMontageWait();

	/** 적용을 기다리는 애님 블루프린트 (없으면 null) */
	UPROPERTY()
	TSubclassOf<UAnimInstance> PendingAnimClass;

	FTimerHandle PendingAnimClassTimer;

	/** 모든 몽타주 종료를 기다리며 바인딩한 애님 인스턴스 */
	TWeakObjectPtr<UAnimInstance> MontageWaitInstance;

	/** 애님 블루프린트를 한 번이라도 적용했는가. 첫 적용(시작 시)은 이전 자세가 기준 자세뿐이라 섞지 않는다. */
	bool bAppliedAnimClassOnce = false;
};
