// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "FloatPropertyTrackEditor.h"
#include "FloatPropertySection.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackFloatBase.h"


TSharedRef<ISequencerTrackEditor> FFloatPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FFloatPropertyTrackEditor(OwningSequencer));
}


TSharedRef<FPropertySection> FFloatPropertyTrackEditor::MakePropertySectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	return MakeShareable(new FFloatPropertySection(SectionObject, Track.GetDisplayName()));
}


void FFloatPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<float>& NewGeneratedKeys, TArray<float>& DefaultGeneratedKeys )
{
	NewGeneratedKeys.Add( PropertyChangedParams.GetPropertyValue<float>() );
}


void CopyInterpFloatTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackFloatBase* MatineeFloatTrack, UMovieSceneFloatTrack* FloatTrack)
{
	if (FMatineeImportTools::CopyInterpFloatTrack(MatineeFloatTrack, FloatTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void FFloatPropertyTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackFloatBase* MatineeFloatTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		MatineeFloatTrack = Cast<UInterpTrackFloatBase>( CopyPasteObject );
		if ( MatineeFloatTrack != nullptr )
		{
			break;
		}
	}
	UMovieSceneFloatTrack* FloatTrack = Cast<UMovieSceneFloatTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeFloatTrack", "Paste Matinee Float Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeFloatTrackTooltip", "Pastes keys from a Matinee float track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &CopyInterpFloatTrack, GetSequencer().ToSharedRef(), MatineeFloatTrack, FloatTrack ),
			FCanExecuteAction::CreateLambda( [=]()->bool { return MatineeFloatTrack != nullptr && MatineeFloatTrack->GetNumKeys() > 0 && FloatTrack != nullptr; } ) ) );
}
