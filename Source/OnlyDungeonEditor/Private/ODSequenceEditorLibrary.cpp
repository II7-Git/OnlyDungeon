// OnlyDungeon 에디터 도구: 레벨 시퀀스 설정

#include "ODSequenceEditorLibrary.h"

#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneCameraCutTrack.h"

namespace ODSequenceEditor
{
	UMovieSceneCameraCutTrack* FindCameraCutTrack(ULevelSequence* Sequence)
	{
		UMovieScene* MovieScene = IsValid(Sequence) ? Sequence->GetMovieScene() : nullptr;
		return MovieScene ? Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack()) : nullptr;
	}
}

bool UODSequenceEditorLibrary::SetCameraCutCanBlend(ULevelSequence* Sequence, bool bCanBlend)
{
	UMovieSceneCameraCutTrack* Track = ODSequenceEditor::FindCameraCutTrack(Sequence);
	if (!Track)
	{
		return false;
	}
	Track->Modify();
	Track->bCanBlend = bCanBlend;
	Sequence->MarkPackageDirty();
	return true;
}

bool UODSequenceEditorLibrary::GetCameraCutCanBlend(ULevelSequence* Sequence)
{
	const UMovieSceneCameraCutTrack* Track = ODSequenceEditor::FindCameraCutTrack(Sequence);
	return Track ? Track->bCanBlend : false;
}
