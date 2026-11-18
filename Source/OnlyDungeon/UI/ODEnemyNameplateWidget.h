// OnlyDungeon 일반 몬스터 머리 위 이름·체력 표시

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "ODEnemyNameplateWidget.generated.h"

class AODCharacterBase;
class UAbilitySystemComponent;
class UProgressBar;
class USizeBox;
class UTextBlock;

/**
 * 일반 몬스터 머리 위에 뜨는 이름표 (이름 + 작은 체력 바).
 * 적 캐릭터의 위젯 컴포넌트(화면 공간)가 이 위젯을 띄우고, 보일지 말지는 적 캐릭터가 정한다. (AODEnemyCharacter::RefreshNameplate)
 * 보스는 이름표 대신 화면 하단 보스 체력 바를 쓴다. (UODHUDWidget)
 * 체력 변경 이벤트로만 갱신한다. (Tick 사용 안 함)
 * BP로 상속해 디자이너에 같은 이름의 위젯을 배치하면 그것을 쓰고, 비어 있으면 C++이 기본 레이아웃을 만든다.
 */
UCLASS()
class ONLYDUNGEON_API UODEnemyNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 표시할 적을 정한다. (이름 설정, 체력 이벤트 연결) */
	void SetCharacter(AODCharacterBase* InCharacter);

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> BarBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthFill;

	/** 체력 바 크기 (px) */
	UPROPERTY(EditDefaultsOnly, Category = "OD|Nameplate")
	FVector2D BarSize = FVector2D(110.0, 7.0);

	UPROPERTY(EditDefaultsOnly, Category = "OD|Nameplate")
	FLinearColor HealthColor = FLinearColor(0.55f, 0.04f, 0.03f);

	UPROPERTY(EditDefaultsOnly, Category = "OD|Nameplate")
	FLinearColor FrameColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

private:
	void BuildDefaultLayout();
	void Bind();
	void Unbind();
	void RefreshHealth();

	/** 표시 대상. 위젯이 화면에 다시 올라올 때 체력 이벤트를 다시 연결하는 데 쓴다. */
	TWeakObjectPtr<AODCharacterBase> ObservedCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedASC;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;
};
