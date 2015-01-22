// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "SubMovieSceneTrackEditor.h"
#include "SubMovieSceneTrack.h"
#include "SubMovieSceneSection.h"

/**
 * A generic implementation for displaying simple property sections
 */
class FSubMovieSceneSection : public ISequencerSection
{
public:
	FSubMovieSceneSection( TSharedPtr<ISequencer> InSequencer, UMovieSceneSection& InSectionObject, FName SectionName )
		: DisplayName( NSLOCTEXT("SubMovieSceneSection", "DisplayName", "Scenes") )
		, SectionObject( *CastChecked<USubMovieSceneSection>( &InSectionObject ) )
		, Sequencer( InSequencer )
	{
		MovieSceneInstance = InSequencer->GetInstanceForSubMovieSceneSection( InSectionObject );
	}

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override { return &SectionObject; }

	virtual FText GetDisplayName() const override
	{
		return DisplayName;
	}

	virtual FText GetSectionTitle() const override
	{
		return FText::FromString( SectionObject.GetMovieScene()->GetName() );
	}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
	}

	virtual FReply OnSectionDoubleClicked( const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent ) override
	{
		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			Sequencer.Pin()->FocusSubMovieScene( MovieSceneInstance.Pin().ToSharedRef() );
		}
		return FReply::Handled();
	}

	virtual float GetSectionHeight() const override { return 30.0f; }

	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override 
	{
		// Add a box for the section
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect,
			ESlateDrawEffect::None,
			FColor( 220, 120, 120 )
		);

		return LayerId;
	}
	
private:
	/** Display name of the section */
	FText DisplayName;
	/** The section we are visualizing */
	USubMovieSceneSection& SectionObject;
	/** The instance that this section is part of */
	TWeakPtr<FMovieSceneInstance> MovieSceneInstance;
	/** Sequencer interface */
	TWeakPtr<ISequencer> Sequencer;
};

FSubMovieSceneTrackEditor::FSubMovieSceneTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{

}


TSharedRef<FMovieSceneTrackEditor> FSubMovieSceneTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FSubMovieSceneTrackEditor( InSequencer ) );
}

bool FSubMovieSceneTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support sub movie scenes
	return Type == USubMovieSceneTrack::StaticClass();
}

TSharedRef<ISequencerSection> FSubMovieSceneTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack* Track )
{
	return MakeShareable( new FSubMovieSceneSection( GetSequencer(), SectionObject, Track->GetTrackName() ) );
}

bool FSubMovieSceneTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	if (Asset->IsA<UMovieScene>())
	{
		GetSequencer()->AddSubMovieScene( CastChecked<UMovieScene>( Asset ) );

		return true;
	}
	return false;
}


