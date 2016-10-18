// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "WidgetMaterialTrackEditor.h"
#include "MovieSceneWidgetMaterialTrack.h"
#include "WidgetMaterialTrackUtilities.h"


FWidgetMaterialTrackEditor::FWidgetMaterialTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMaterialTrackEditor( InSequencer )
{
}


TSharedRef<ISequencerTrackEditor> FWidgetMaterialTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable( new FWidgetMaterialTrackEditor( OwningSequencer ) );
}


bool FWidgetMaterialTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneWidgetMaterialTrack::StaticClass();
}


UMaterialInterface* FWidgetMaterialTrackEditor::GetMaterialInterfaceForTrack( FGuid ObjectBinding, UMovieSceneMaterialTrack* MaterialTrack )
{
	UObject* WidgetObject = GetSequencer()->GetFocusedMovieSceneSequenceInstance()->FindObject( ObjectBinding, *GetSequencer() );
	UWidget* Widget = Cast<UWidget>( WidgetObject );
	UMovieSceneWidgetMaterialTrack* WidgetMaterialTrack = Cast<UMovieSceneWidgetMaterialTrack>( MaterialTrack );
	if ( Widget != nullptr && WidgetMaterialTrack != nullptr )
	{
		FSlateBrush* Brush = WidgetMaterialTrackUtilities::GetBrush( Widget, WidgetMaterialTrack->GetBrushPropertyNamePath() );
		if ( Brush != nullptr )
		{
			return Cast<UMaterialInterface>(Brush->GetResourceObject());
		}
	}
	return nullptr;
}
