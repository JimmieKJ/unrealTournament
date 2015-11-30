// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneSpawnTrack.h"
#include "MovieSceneTrack.h"
#include "SpawnTrackEditor.h"


#define LOCTEXT_NAMESPACE "FSpawnTrackEditor"


TSharedRef<ISequencerTrackEditor> FSpawnTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FSpawnTrackEditor(InSequencer));
}


FSpawnTrackEditor::FSpawnTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FBoolPropertyTrackEditor(InSequencer)
{ }


UMovieSceneTrack* FSpawnTrackEditor::AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<UMovieSceneTrack> TrackClass, FName UniqueTypeName)
{
	UMovieSceneTrack* NewTrack = FBoolPropertyTrackEditor::AddTrack(FocusedMovieScene, ObjectHandle, TrackClass, UniqueTypeName);

	if (auto* SpawnTrack = Cast<UMovieSceneSpawnTrack>(NewTrack))
	{
		SpawnTrack->SetObjectId(ObjectHandle);
		SpawnTrack->AddSection(*SpawnTrack->CreateNewSection());
	}

	return NewTrack;
}

void FSpawnTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	UMovieSceneSpawnTrack* SpawnTrack = Cast<UMovieSceneSpawnTrack>(Track);
	if (!SpawnTrack)
	{
		return;
	}

	FMovieSceneSpawnable* Spawnable = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(SpawnTrack->GetObjectId());
	if (!Spawnable)
	{
		return;
	}

	MenuBuilder.AddSubMenu(
		LOCTEXT("OwnerLabel", "Spawned Object Owner"),
		LOCTEXT("OwnerTooltip", "Specifies how the object spawned in this track is to be owned"),
		FNewMenuDelegate::CreateSP(this, &FSpawnTrackEditor::AddSpawnOwnershipMenu, SpawnTrack)
	);
}

void FSpawnTrackEditor::AddSpawnOwnershipMenu(FMenuBuilder& MenuBuilder, UMovieSceneSpawnTrack* Track)
{
	FMovieSceneSpawnable* Spawnable = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(Track->GetObjectId());

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ThisSequence_Label", "This Sequence"),
		LOCTEXT("ThisSequence_Tooltip", "Indicates that this sequence will own the spawned object. The object will be destroyed at the end of the sequence."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Spawnable->SetSpawnOwnership(ESpawnOwnership::InnerSequence); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::InnerSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MasterSequence_Label", "Master Sequence"),
		LOCTEXT("MasterSequence_Tooltip", "Indicates that the outermost sequence will own the spawned object. The object will be destroyed when the outermost sequence stops playing."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Spawnable->SetSpawnOwnership(ESpawnOwnership::MasterSequence); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::MasterSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("External_Label", "External"),
		LOCTEXT("External_Tooltip", "Indicates this object's lifetime is managed externally once spawned. It will not be destroyed by sequencer."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]{ Spawnable->SetSpawnOwnership(ESpawnOwnership::External); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::External; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
}

void FSpawnTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	UMovieSceneSequence* MovieSequence = GetSequencer()->GetFocusedMovieSceneSequence();

	if (!MovieSequence || MovieSequence->GetClass()->GetName() != TEXT("LevelSequence") || !MovieSequence->GetMovieScene()->FindSpawnable(ObjectBinding))
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddSpawnTrack", "Spawn Track"),
		LOCTEXT("AddSpawnTrackTooltip", "Adds a new track that controls the lifetime of the track's spawnable object."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FSpawnTrackEditor::HandleAddSpawnTrackMenuEntryExecute, ObjectBinding),
			FCanExecuteAction::CreateSP(this, &FSpawnTrackEditor::CanAddSpawnTrack, ObjectBinding)
		)
	);
}


bool FSpawnTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneSpawnTrack::StaticClass());
}


void FSpawnTrackEditor::HandleAddSpawnTrackMenuEntryExecute(FGuid ObjectBinding)
{
	FScopedTransaction AddSpawnTrackTransaction(LOCTEXT("AddSpawnTrack_Transaction", "Add Spawn Track"));
	AddTrack(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectBinding, UMovieSceneSpawnTrack::StaticClass(), NAME_None);
	NotifyMovieSceneDataChanged();
}


bool FSpawnTrackEditor::CanAddSpawnTrack(FGuid ObjectBinding) const
{
	return !GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieSceneSpawnTrack>(ObjectBinding);
}


#undef LOCTEXT_NAMESPACE
