// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "Sequencer2DTransformTrackEditor.h"
#include "Developer/MovieSceneTools/Public/PropertySection.h"
#include "Developer/MovieSceneTools/Public/MovieSceneToolHelpers.h"
#include "Runtime/UMG/Public/Animation/MovieScene2DTransformSection.h"
#include "Runtime/UMG/Public/Animation/MovieScene2DTransformTrack.h"
#include "Editor/Sequencer/Public/ISectionLayoutBuilder.h"
#include "Editor/Sequencer/Public/ISequencerObjectChangeListener.h"
#include "Editor/PropertyEditor/Public/PropertyHandle.h"
#include "Slate/WidgetTransform.h"

class F2DTransformSection : public FPropertySection
{
public:
	F2DTransformSection( UMovieSceneSection& InSectionObject, FName SectionName )
		: FPropertySection(InSectionObject, SectionName) {}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		UMovieScene2DTransformSection* TransformSection = Cast<UMovieScene2DTransformSection>(&SectionObject);

		// This generates the tree structure for the transform section
		LayoutBuilder.PushCategory("Location", NSLOCTEXT("F2DTransformSection", "LocationArea", "Location"));
			LayoutBuilder.AddKeyArea("Location.X", NSLOCTEXT("F2DTransformSection", "LocXArea", "X"), MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::X), TransformSection)));
			LayoutBuilder.AddKeyArea("Location.Y", NSLOCTEXT("F2DTransformSection", "LocYArea", "Y"), MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::Y), TransformSection)));
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Rotation", NSLOCTEXT("F2DTransformSection", "RotationArea", "Rotation"));
			LayoutBuilder.AddKeyArea("Rotation.Angle", NSLOCTEXT("F2DTransformSection", "AngleArea", "Angle"), MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetRotationCurve(), TransformSection)));
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Scale", NSLOCTEXT("F2DTransformSection", "ScaleArea", "Scale"));
			LayoutBuilder.AddKeyArea("Scale.X", NSLOCTEXT("F2DTransformSection", "ScaleXArea", "X"), MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::X), TransformSection)));
			LayoutBuilder.AddKeyArea("Scale.Y", NSLOCTEXT("F2DTransformSection", "ScaleYArea", "Y"), MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::Y), TransformSection)));
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Sheer", NSLOCTEXT("F2DTransformSection", "SheerArea", "Sheer"));
			LayoutBuilder.AddKeyArea("Sheer.X", NSLOCTEXT("F2DTransformSection", "SheerXArea", "X"), MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetSheerCurve(EAxis::X), TransformSection)));
			LayoutBuilder.AddKeyArea("Sheer.Y", NSLOCTEXT("F2DTransformSection", "SheerYArea", "Y"), MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetSheerCurve(EAxis::Y), TransformSection)));
		LayoutBuilder.PopCategory();
	}
};

F2DTransformTrackEditor::F2DTransformTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{
	// Get the object change listener for the sequencer and register a delegates for when properties change that we care about
	ISequencerObjectChangeListener& ObjectChangeListener = InSequencer->GetObjectChangeListener();
	ObjectChangeListener.GetOnAnimatablePropertyChanged( "WidgetTransform" ).AddRaw( this, &F2DTransformTrackEditor::OnTransformChanged );
}

F2DTransformTrackEditor::~F2DTransformTrackEditor()
{
	TSharedPtr<ISequencer> Sequencer = GetSequencer();
	if( Sequencer.IsValid() )
	{
		ISequencerObjectChangeListener& ObjectChangeListener = Sequencer->GetObjectChangeListener();
		ObjectChangeListener.GetOnAnimatablePropertyChanged( "WidgetTransform" ).RemoveAll( this );
	}
}



TSharedRef<FMovieSceneTrackEditor> F2DTransformTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F2DTransformTrackEditor( InSequencer ) );
}

bool F2DTransformTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieScene2DTransformTrack::StaticClass();
}

TSharedRef<ISequencerSection> F2DTransformTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack* Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	UClass* SectionClass = SectionObject.GetOuter()->GetClass();

	TSharedRef<ISequencerSection> NewSection = MakeShareable( new F2DTransformSection( SectionObject, Track->GetTrackName() ) );

	return NewSection;
}


void F2DTransformTrackEditor::OnTransformChanged( const FKeyPropertyParams& PropertyKeyParams )
{
	FName PropertyName = PropertyKeyParams.PropertyHandle->GetProperty()->GetFName();

	AnimatablePropertyChanged
	(
		UMovieScene2DTransformTrack::StaticClass(), 
		PropertyKeyParams.bRequireAutoKey,
		FOnKeyProperty::CreateRaw(this, &F2DTransformTrackEditor::OnKeyTransform, &PropertyKeyParams ) 
	);
}


void F2DTransformTrackEditor::OnKeyTransform( float KeyTime, const FKeyPropertyParams* PropertyKeyParams )
{
	TArray<const void*> TransformValues;
	PropertyKeyParams->PropertyHandle->AccessRawData( TransformValues );

	FName PropertyName = PropertyKeyParams->PropertyHandle->GetProperty()->GetFName();

	for( int32 ObjectIndex = 0; ObjectIndex < PropertyKeyParams->ObjectsThatChanged.Num(); ++ObjectIndex )
	{
		UObject* Object = PropertyKeyParams->ObjectsThatChanged[ObjectIndex];
		FWidgetTransform TransformValue = *(const FWidgetTransform*)TransformValues[ObjectIndex];

		F2DTransformKey Key;
		Key.bAddKeyEvenIfUnchanged = !PropertyKeyParams->bRequireAutoKey;
		Key.CurveName = PropertyKeyParams->InnerStructPropertyName;
		Key.Value = TransformValue;

		FGuid ObjectHandle = FindOrCreateHandleToObject( Object );
		if (ObjectHandle.IsValid())
		{
			UMovieSceneTrack* Track = GetTrackForObject( ObjectHandle, UMovieScene2DTransformTrack::StaticClass(), PropertyName );
			if( ensure( Track ) )
			{
				UMovieScene2DTransformTrack* TransformTrack = CastChecked<UMovieScene2DTransformTrack>(Track);
				TransformTrack->SetPropertyNameAndPath( PropertyName, PropertyKeyParams->PropertyPath );
				// Find or add a new section at the auto-key time and changing the property same property
				// AddKeyToSection is not actually a virtual, it's redefined in each class with a different type
				bool bSuccessfulAdd = TransformTrack->AddKeyToSection( KeyTime, Key );
				if (bSuccessfulAdd)
				{
					TransformTrack->SetAsShowable();
				}
			}
		}
	}
}