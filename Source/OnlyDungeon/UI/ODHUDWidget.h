// OnlyDungeon 플레이어 HUD 위젯 (다크소울식 배치)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "ODHUDWidget.generated.h"

class APawn;
class UAbilitySystemComponent;
class UCanvasPanel;
class UODCombatModeComponent;
class UODInteractionComponent;
class UODLockOnComponent;
class UODPotionComponent;
class AODEnemyCharacter;
class AODCharacterBase;
class UGameplayAbility;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UWidget;
struct FGameplayEventData;

/** 바 하나의 표시 상태 (현재값, 최대치, 지연 감소 중인 값) */
struct FODHUDBarState
{
	float Current = 0.0f;
	float Max = 1.0f;
	float Lag = 0.0f;
	double LagHoldUntil = 0.0;
	bool bInitialized = false;
};

/**
 * 플레이어 HUD. 다크소울 배치를 따른다.
 * - 왼쪽 위: 체력(빨강)·스태미나(초록) 바. 길이는 최대치에 비례하고, 체력은 깎인 부분이 잠시 남았다가 줄어든다.
 * - 왼쪽 아래: 장비 십자 슬롯 (오른손 = 현재 무기, 아래 = 회복 물약과 남은 개수)
 * - 아래 가운데: 보스 체력 바 (보스전 중에만 보스 이름·체력. 보스전 서브시스템의 시작·종료 알림)
 *   일반 몬스터의 체력은 머리 위 이름표(UODEnemyNameplateWidget)로 보여 준다.
 * - 락온 대상 위: 락온 표식 (락온 중에만 타이머로 화면 위치 갱신)
 * - 아래 가운데(대상 바 위): 상호작용 안내 ("F  보스방 입장")
 * - 가운데: 사망 문구 (플레이어가 죽어 있는 동안)
 * - 컷신 중에는 HUD 전체를 숨긴다. (전투 모드 컴포넌트의 OnCinematicChanged)
 * - 오른쪽 위: 테스트 정보 (상태 태그, 몽타주·섹션, 강인도, 최근 발동 실패 원인). 콘솔 OD.Debug.HUD 0/1
 *
 * WBP(WBP_OD_HUD)가 이 클래스를 부모로 쓴다. 디자이너에 같은 이름의 위젯을 배치하면 그것을 쓰고,
 * 비어 있으면 C++이 기본 레이아웃을 만든다.
 * 갱신: 바는 어트리뷰트 변경 이벤트로, 지연 감소는 진행 중일 때만 타이머로, 테스트 정보는 0.1초 타이머로 갱신한다. (Tick 사용 안 함)
 */
UCLASS()
class ONLYDUNGEON_API UODHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 폰을 바꾼다. (nullptr이면 연결 해제) */
	void SetObservedPawn(APawn* NewPawn);

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---- 위젯 (디자이너에 같은 이름으로 배치하면 그것을 쓴다) ----

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> HealthBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthFill;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthLag;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> StaminaBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaFill;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RightHandText;

	/** 아래 슬롯: 회복 물약 이름 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	/** 아래 슬롯: 회복 물약 남은 개수 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TargetPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TargetNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> TargetFill;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> TargetLag;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> InteractionPrompt;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InteractionText;

	/** 보스 등장 시 크게 띄우는 보스 이름 (가운데 위) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BossTitleText;

	/** 보스를 쓰러뜨렸을 때 띄우는 문구 (가운데) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> VictoryText;

	/** 사망 문구 (가운데) */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DeathText;

	/** 락온 대상 위에 띄우는 표식 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockOnMarker;

	/** 상호작용 키 표시 (키 설정 화면이 생기면 입력 매핑에서 가져온다) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Interaction")
	FText InteractionKeyLabel = FText::FromString(TEXT("F"));

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DebugPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DebugText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FailureText;

	// ---- 설정 ----

	/** 체력·스태미나 1당 바 길이(px). 다크소울처럼 최대치가 오르면 바가 길어진다. */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Bar", meta = (ClampMin = "0.1"))
	float PixelsPerPoint = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Bar")
	FVector2D BarWidthRange = FVector2D(150.0, 900.0);

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Bar")
	float BarHeight = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Bar")
	float TargetBarWidth = 800.0f;

	/** 깎인 체력이 남아 있다가 줄어들기 시작할 때까지(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Bar", meta = (ClampMin = "0.0"))
	float LagDelay = 0.6f;

	/** 지연 바가 줄어드는 속도 (초당 최대치 대비 비율) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Bar", meta = (ClampMin = "0.05"))
	float LagSpeed = 0.8f;

	/** 마지막 타격 후 대상 바를 숨길 때까지(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Target", meta = (ClampMin = "0.5"))
	float TargetHideDelay = 8.0f;

	/** 발동 실패 원인을 보여줄 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Debug", meta = (ClampMin = "0.2"))
	float FailureShowTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Color")
	FLinearColor HealthColor = FLinearColor(0.55f, 0.04f, 0.03f);

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Color")
	FLinearColor StaminaColor = FLinearColor(0.12f, 0.45f, 0.10f);

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Color")
	FLinearColor LagColor = FLinearColor(0.85f, 0.65f, 0.15f);

	UPROPERTY(EditDefaultsOnly, Category = "OD|HUD|Color")
	FLinearColor FrameColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

private:
	/** WBP에 배치된 위젯이 없을 때 기본 레이아웃을 만든다. */
	void BuildDefaultLayout(UCanvasPanel* Canvas);

	void BindToASC(UAbilitySystemComponent* ASC);
	void UnbindFromASC();
	void SetTarget(AActor* NewTarget);
	void ClearTarget();

	void RefreshPlayerBars();
	void RefreshTargetBar();
	void RefreshDebugInfo();
	void UpdateLag();

	/** 바 상태 갱신. bSizeByMax면 최대치에 비례해 바 길이를 바꾼다. */
	void ApplyBar(FODHUDBarState& State, float NewCurrent, float NewMax, UProgressBar* Fill, UProgressBar* Lag, USizeBox* Box, UTextBlock* ValueText, bool bUseLag);
	void StartLagTimer();

	void HandleActivationFailed(const UGameplayAbility* Ability, const FString& Reason);

	UFUNCTION()
	void HandleCinematicChanged(bool bInCinematic);

	UFUNCTION()
	void HandleInteractionFocusChanged(AActor* NewFocus);

	UFUNCTION()
	void HandleLockOnTargetChanged(AActor* NewTarget);

	UFUNCTION()
	void HandlePotionChargesChanged(int32 CurrentCharges, int32 MaxCharges);

	UFUNCTION()
	void HandleBossEncounterStarted(AODEnemyCharacter* Boss);

	UFUNCTION()
	void HandleBossEncounterEnded(bool bVictory);

	UFUNCTION()
	void HandleObservedPawnDeath(AODCharacterBase* DeadCharacter);

	/** 큰 문구를 서서히 나타냈다가 사라지게 한다. (실제 시간 기준이라 느린 화면에도 같은 속도) */
	void ShowBanner(UTextBlock* Banner, double& OutStartTime, float Delay);
	void UpdateBanners();
	/** 한 문구의 현재 불투명도. 끝났으면 음수 */
	float GetBannerOpacity(double StartTime, double Now) const;

	/** 사망 문구를 관찰 중인 폰의 사망 여부에 맞춘다. */
	void RefreshDeathText();

	/** 락온 표식을 대상의 화면 위치로 옮긴다. */
	void UpdateLockOnMarker();

	/** 대상 바를 TargetHideDelay 뒤에 숨기도록 예약한다. */
	void StartTargetHideTimer();

	void BindPawnComponents(APawn* Pawn);
	void UnbindPawnComponents();

	TWeakObjectPtr<APawn> ObservedPawn;
	TWeakObjectPtr<UODCombatModeComponent> ObservedModeComponent;
	TWeakObjectPtr<UODInteractionComponent> ObservedInteraction;
	TWeakObjectPtr<UODLockOnComponent> ObservedLockOn;
	TWeakObjectPtr<UODPotionComponent> ObservedPotion;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> PlayerAttributeHandles;
	FDelegateHandle FailureHandle;

	/** 보스전 중: 대상 바가 보스에 고정되어 다른 대상으로 바뀌거나 숨겨지지 않는다. (대상 바는 보스 전용) */
	bool bBossBarPinned = false;

	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> TargetAttributeHandles;

	FODHUDBarState HealthState;
	FODHUDBarState StaminaState;
	FODHUDBarState TargetState;

	FTimerHandle LagTimer;
	FTimerHandle DebugTimer;
	FTimerHandle TargetHideTimer;
	FTimerHandle FailureHideTimer;
	FTimerHandle LockOnMarkerTimer;
	FTimerHandle BannerTimer;

	/** 문구 시작 시각 (실제 시간, 음수면 표시 안 함) */
	double BossTitleStart = -1.0;
	double VictoryStart = -1.0;

	TWeakObjectPtr<const UObject> LastWeaponData;
};
