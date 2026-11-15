// OnlyDungeon 애님 노티파이 스테이트: 구간 시작·끝에 게임플레이 이벤트 전송

#include "ODAnimNotifyState_GameplayEventWindow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SkeletalMeshComponent.h"

void UODAnimNotifyState_GameplayEventWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	SendWindowEvent(MeshComp, true);
}

void UODAnimNotifyState_GameplayEventWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	SendWindowEvent(MeshComp, false);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UODAnimNotifyState_GameplayEventWindow::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? EventTag.ToString() : Super::GetNotifyName_Implementation();
}

void UODAnimNotifyState_GameplayEventWindow::SendWindowEvent(const USkeletalMeshComponent* MeshComp, bool bOpen) const
{
	if (!IsValid(MeshComp) || !EventTag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (!IsValid(ASC))
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;
	Payload.EventMagnitude = bOpen ? 1.0f : 0.0f;
	ASC->HandleGameplayEvent(EventTag, &Payload);
}
