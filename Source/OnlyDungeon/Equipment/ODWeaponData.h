// OnlyDungeon 무기 데이터

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ODWeaponData.generated.h"

class AODWeaponActor;
class UAnimInstance;
class UODAbilitySet;
class UStaticMesh;
class UMaterialInterface;
class USoundBase;

/**
 * 무기 하나를 정의하는 데이터 에셋. 에셋 접두사: DA_Weapon_ (예: DA_Weapon_OneHandSword)
 * 장착하면 메시를 손에 붙이고, 어빌리티 세트(공격·방어·스킬 + 스탯 GE)를 부여하고,
 * 무기 타입 태그와 이동 애니메이션 레이어를 적용한다.
 */
UCLASS(BlueprintType, Const)
class ONLYDUNGEON_API UODWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 화면에 표시할 무기 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon")
	FText DisplayName;

	/** HUD 오른손 칸에 보일 아이콘 (UI 재질, Tools/UI/build_item_icons.py가 메시를 찍어 만든다). 비우면 이름 글자만 보인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|UI")
	TSoftObjectPtr<UMaterialInterface> HudIcon;

	/** 무기 타입 (Weapon.Type.*). 장착 중 ASC에 태그로 붙는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon", meta = (Categories = "Weapon.Type"))
	FGameplayTag WeaponType;

	/** 스폰할 무기 액터 클래스 (비우면 AODWeaponActor) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon")
	TSubclassOf<AODWeaponActor> WeaponActorClass;

	/** 무기 메시 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Visual")
	TObjectPtr<UStaticMesh> WeaponMesh;

	/** 캐릭터 메시에서 무기를 붙일 소켓(또는 본) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Visual")
	FName AttachSocket = TEXT("hand_r");

	/** 소켓 기준 무기 위치·회전 보정 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Visual")
	FTransform AttachOffset = FTransform::Identity;

	/**
	 * 공격·방어 어빌리티가 실행되는 동안 무기를 붙일 소켓(본). 비우면 바꾸지 않는다.
	 * 원본 모션이 무기 본을 따로 움직이는 경우(예: Kwang 대검) 그 움직임을 구운 본(ik_hand_gun)을 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Visual")
	FName AbilityAttachSocket;

	/** AbilityAttachSocket 기준 무기 위치·회전 보정 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Visual")
	FTransform AbilityAttachOffset = FTransform::Identity;

	/** 왼손에 함께 드는 장식용 메시 (예: 골렘의 방패). 판정 없이 보이기만 한다. 비우면 없음 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Offhand")
	TObjectPtr<UStaticMesh> OffhandMesh;

	/** 왼손 메시를 붙일 소켓(본) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Offhand")
	FName OffhandAttachSocket = TEXT("hand_l");

	/** 왼손 메시 위치·회전 보정 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Offhand")
	FTransform OffhandAttachOffset = FTransform::Identity;

	/** 장착 시 부여할 어빌리티 세트 (공격·방어·스킬, 스탯 GE) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Ability")
	TObjectPtr<UODAbilitySet> AbilitySet;

	/**
	 * 장착 중 사용할 애님 블루프린트 (무기별 대기·이동 모션). 비우면 캐릭터 기본 애님 블루프린트 유지.
	 * 무기별 ABP는 ABP_OD_Humanoid를 복제해 대기 시퀀스와 이동 블렌드스페이스만 바꾼다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Animation")
	TSubclassOf<UAnimInstance> AnimInstanceClass;

	/** 장착 중 연결할 이동 애니메이션 레이어 (Linked Anim Layer). 비우면 기본 이동 유지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Animation")
	TSubclassOf<UAnimInstance> AnimLayerClass;

	/** 판정 시작점 (무기 메시 로컬 좌표, 보통 칼날 시작) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Trace")
	FVector TraceStart = FVector(0.0, 0.0, -15.0);

	/** 판정 끝점 (무기 메시 로컬 좌표, 보통 칼끝) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Trace")
	FVector TraceEnd = FVector(0.0, 0.0, -100.0);

	/** 판정 구의 반지름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Trace", meta = (ClampMin = "1.0"))
	float TraceRadius = 8.0f;

	/** 칼날을 따라 나눌 판정 지점 수 (2 이상) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Trace", meta = (ClampMin = "2", ClampMax = "16"))
	int32 TraceSamples = 4;

	/**
	 * 보조 손 판정: 양손에 무기를 든 몬스터(양팔 칼날, 쌍철퇴)처럼 왼손 공격도 맞혀야 할 때 켠다.
	 * 소유자 메시의 OffhandAttachSocket 기준 OffhandTraceStart~End 구간을 같은 반경·지점 수로 함께 쓸어 낸다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Trace")
	bool bTraceOffhand = false;

	/** 보조 손 판정 시작점 (OffhandAttachSocket 기준) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Trace", meta = (EditCondition = "bTraceOffhand"))
	FVector OffhandTraceStart = FVector::ZeroVector;

	/** 보조 손 판정 끝점 (OffhandAttachSocket 기준) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Trace", meta = (EditCondition = "bTraceOffhand"))
	FVector OffhandTraceEnd = FVector::ZeroVector;

	/** 판정을 아래쪽으로 늘리는 길이. 덩치 큰 적의 휘두르기가 사람 머리 위로 지나가지 않게 한다. (0이면 구 판정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceDownReach = 0.0f;

	/** 필드 모드 범위 판정: 캐릭터 중심에서의 반경 (칼날 스윕 대신 전방 부채꼴로 여러 적을 한 번에 맞힌다) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Field", meta = (ClampMin = "10.0", Units = "cm"))
	float FieldHitRadius = 220.0f;

	/** 필드 모드 범위 판정: 캐릭터 정면 기준 좌우 각도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Field", meta = (ClampMin = "5.0", ClampMax = "180.0", Units = "deg"))
	float FieldHitHalfAngle = 60.0f;

	/**
	 * 휘두르는 소리. 공격 모션의 무기 판정 구간이 시작될 때(UODAnimNotifyState_WeaponTrace) 무기에 붙여 한 번 재생한다.
	 * 공격 몽타주마다 노티파이를 따로 달지 않아도 모든 공격에 소리가 난다. 여러 소리를 번갈아 내려면 SoundCue(Random 노드)를 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "OD|Weapon|Audio")
	TObjectPtr<USoundBase> SwingSound;
};
