// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneFadeTrack.h"
#include "MovieSceneTrack.h"
#include "FadeTrackEditor.h"


#define LOCTEXT_NAMESPACE "FFadeTrackEditor"


/* FFadeTrackEditor static functions
 *****************************************************************************/

TSharedRef<ISequencerTrackEditor> FFadeTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FFadeTrackEditor(InSequencer));
}


/* FFadeTrackEditor structors
 *****************************************************************************/

FFadeTrackEditor::FFadeTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FFloatPropertyTrackEditor(InSequencer)
{ }


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FFadeTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	UMovieSceneSequence* RootMovieSceneSequence = GetSequencer()->GetRootMovieSceneSequence();

	if ((RootMovieSceneSequence == nullptr) || (RootMovieSceneSequence->GetClass()->GetName() != TEXT("LevelSequence")))
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddFadeTrack", "Fade Track"),
		LOCTEXT("AddFadeTrackTooltip", "Adds a new track that controls the fade of the sequence."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Fade"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FFadeTrackEditor::HandleAddFadeTrackMenuEntryExecute)
		)
	);
}


bool FFadeTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneFadeTrack::StaticClass());
}


/* FFadeTrackEditor callbacks
 *****************************************************************************/

void FFadeTrackEditor::HandleAddFadeTrackMenuEntryExecute()
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	if (MovieScene == nullptr)
	{
		return;
	}

	UMovieSceneTrack* FadeTrack = MovieScene->FindMasterTrack<UMovieSceneFadeTrack>();

	if (FadeTrack != nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddFadeTrack_Transaction", "Add Fade Track"));

	MovieScene->Modify();

	FadeTrack = FindOrCreateMasterTrack<UMovieSceneFadeTrack>().Track;
	ensure(FadeTrack);

	UMovieSceneSection* NewSection = FadeTrack->CreateNewSection();
	ensure(NewSection);

	FadeTrack->AddSection(*NewSection);

	GetSequencer()->NotifyMovieSceneDataChanged();
}


#undef LOCTEXT_NAMESPACE
