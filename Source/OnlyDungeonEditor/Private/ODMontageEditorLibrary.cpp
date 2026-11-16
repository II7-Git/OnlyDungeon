// OnlyDungeon 에디터 도구: 애님 몽타주 구성

#include "ODMontageEditorLibrary.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogODEditor, Log, All);

bool UODMontageEditorLibrary::BuildSectionedMontage(UAnimMontage* Montage, const TArray<UAnimSequenceBase*>& Sequences, const TArray<FName>& SectionNames, const TArray<FName>& NextSectionNames)
{
	if (!IsValid(Montage) || Sequences.IsEmpty() || SectionNames.Num() != Sequences.Num() || NextSectionNames.Num() != Sequences.Num())
	{
		UE_LOG(LogODEditor, Error, TEXT("BuildSectionedMontage: 인자가 잘못되었습니다. (몽타주 유효성, 시퀀스·섹션·다음 섹션 개수 일치 확인)"));
		return false;
	}

	for (const UAnimSequenceBase* Sequence : Sequences)
	{
		if (!IsValid(Sequence))
		{
			UE_LOG(LogODEditor, Error, TEXT("BuildSectionedMontage: 유효하지 않은 시퀀스가 있습니다."));
			return false;
		}
		if (Sequence->GetSkeleton() != Montage->GetSkeleton())
		{
			UE_LOG(LogODEditor, Warning, TEXT("BuildSectionedMontage: %s의 스켈레톤이 몽타주와 다릅니다."), *Sequence->GetName());
		}
	}

	Montage->Modify();

	if (Montage->SlotAnimTracks.IsEmpty())
	{
		Montage->AddSlot(FName(TEXT("DefaultSlot")));
	}

	FAnimTrack& AnimTrack = Montage->SlotAnimTracks[0].AnimTrack;
	AnimTrack.AnimSegments.Reset();
	Montage->CompositeSections.Reset();

	float Position = 0.0f;
	for (int32 Index = 0; Index < Sequences.Num(); ++Index)
	{
		FAnimSegment Segment;
		Segment.SetAnimReference(Sequences[Index], true);
		Segment.StartPos = Position;
		AnimTrack.AnimSegments.Add(Segment);

		FCompositeSection Section;
		Section.SectionName = SectionNames[Index];
		Section.NextSectionName = NextSectionNames[Index];
		Section.SetTime(Position);
		Montage->CompositeSections.Add(Section);

		Position += Segment.GetLength();
	}

	Montage->SetCompositeLength(Position);

	// 섹션을 세그먼트에 연결하고 캐시를 갱신한다.
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		Section.Link(Montage, Section.GetTime());
	}
	Montage->PostEditChange();
	Montage->MarkPackageDirty();

	UE_LOG(LogODEditor, Log, TEXT("BuildSectionedMontage: %s (세그먼트 %d개, 길이 %.2f초)"), *Montage->GetName(), Sequences.Num(), Position);
	return true;
}

bool UODMontageEditorLibrary::BuildRangeMontage(UAnimMontage* Montage, UAnimSequenceBase* Sequence, float StartTime, float EndTime, FName SlotName, float PlayRate)
{
	if (!IsValid(Montage) || !IsValid(Sequence) || EndTime <= StartTime || PlayRate <= 0.0f || SlotName.IsNone())
	{
		UE_LOG(LogODEditor, Error, TEXT("BuildRangeMontage: 인자가 잘못되었습니다. (몽타주·시퀀스 유효성, 구간, 재생 속도, 슬롯 이름 확인)"));
		return false;
	}

	const float SequenceLength = Sequence->GetPlayLength();
	StartTime = FMath::Clamp(StartTime, 0.0f, SequenceLength);
	EndTime = FMath::Clamp(EndTime, StartTime, SequenceLength);

	Montage->Modify();
	if (Montage->SlotAnimTracks.IsEmpty())
	{
		Montage->AddSlot(SlotName);
	}
	Montage->SlotAnimTracks.SetNum(1);
	Montage->SlotAnimTracks[0].SlotName = SlotName;

	FAnimTrack& AnimTrack = Montage->SlotAnimTracks[0].AnimTrack;
	AnimTrack.AnimSegments.Reset();
	FAnimSegment Segment;
	Segment.SetAnimReference(Sequence, true);
	Segment.StartPos = 0.0f;
	Segment.AnimStartTime = StartTime;
	Segment.AnimEndTime = EndTime;
	Segment.AnimPlayRate = PlayRate;
	AnimTrack.AnimSegments.Add(Segment);

	const float Length = Segment.GetLength();
	Montage->CompositeSections.Reset();
	FCompositeSection Section;
	Section.SectionName = FName(TEXT("Default"));
	Section.SetTime(0.0f);
	Montage->CompositeSections.Add(Section);
	Montage->SetCompositeLength(Length);
	Montage->CompositeSections[0].Link(Montage, 0.0f);

	Montage->PostEditChange();
	Montage->MarkPackageDirty();

	UE_LOG(LogODEditor, Log, TEXT("BuildRangeMontage: %s ← %s [%.2f~%.2f] x%.2f, 슬롯 %s, 길이 %.2f초"), *Montage->GetName(), *Sequence->GetName(), StartTime, EndTime, PlayRate, *SlotName.ToString(), Length);
	return true;
}

float UODMontageEditorLibrary::GetSectionStartTime(const UAnimMontage* Montage, FName SectionName)
{
	if (!IsValid(Montage))
	{
		return -1.0f;
	}

	const int32 SectionIndex = Montage->GetSectionIndex(SectionName);
	if (SectionIndex == INDEX_NONE)
	{
		return -1.0f;
	}

	float StartTime = 0.0f;
	float EndTime = 0.0f;
	Montage->GetSectionStartAndEndTime(SectionIndex, StartTime, EndTime);
	return StartTime;
}
