// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieScene3DTransformTrack.h"
#include "MovieScene3DTransformSection.h"
#include "ISequencerObjectChangeListener.h"
#include "ISequencerSection.h"
#include "ISectionLayoutBuilder.h"
#include "IKeyArea.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneTrackEditor.h"
#include "TransformTrackEditor.h"

/**
 * Class that draws a transform section in the sequencer
 */
class F3DTransformSection : public ISequencerSection
{
public:
	F3DTransformSection( UMovieSceneSection& InSection )
		: Section( InSection )
	{
	}

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}

	virtual FText GetDisplayName() const override
	{ 
		return NSLOCTEXT("FTransformSection", "DisplayName", "Transform");
	}
	
	virtual FText GetSectionTitle() const override { return FText::GetEmpty(); }

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>( &Section );

		// This generates the tree structure for the transform section
		LayoutBuilder.PushCategory( "Location", NSLOCTEXT("FTransformSection", "LocationArea", "Location") );
			LayoutBuilder.AddKeyArea("Location.X", NSLOCTEXT("FTransformSection", "LocXArea", "X"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetTranslationCurve( EAxis::X ), TransformSection ) ) );
			LayoutBuilder.AddKeyArea("Location.Y", NSLOCTEXT("FTransformSection", "LocYArea", "Y"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetTranslationCurve( EAxis::Y ), TransformSection ) ) );
			LayoutBuilder.AddKeyArea("Location.Z", NSLOCTEXT("FTransformSection", "LocZArea", "Z"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetTranslationCurve( EAxis::Z ), TransformSection ) ) );
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory( "Rotation", NSLOCTEXT("FTransformSection", "RotationArea", "Rotation") );
			LayoutBuilder.AddKeyArea("Rotation.X", NSLOCTEXT("FTransformSection", "RotXArea", "X"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetRotationCurve( EAxis::X ), TransformSection ) ) );
			LayoutBuilder.AddKeyArea("Rotation.Y", NSLOCTEXT("FTransformSection", "RotYArea", "Y"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetRotationCurve( EAxis::Y ), TransformSection ) ) );
			LayoutBuilder.AddKeyArea("Rotation.Z", NSLOCTEXT("FTransformSection", "RotZArea", "Z"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetRotationCurve( EAxis::Z ), TransformSection ) ) );
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory( "Scale", NSLOCTEXT("FTransformSection", "ScaleArea", "Scale") );
			LayoutBuilder.AddKeyArea("Scale.X", NSLOCTEXT("FTransformSection", "ScaleXArea", "X"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetScaleCurve( EAxis::X ), TransformSection ) ) );
			LayoutBuilder.AddKeyArea("Scale.Y", NSLOCTEXT("FTransformSection", "ScaleYArea", "Y"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetScaleCurve( EAxis::Y ), TransformSection ) ) );
			LayoutBuilder.AddKeyArea("Scale.Z", NSLOCTEXT("FTransformSection", "ScaleZArea", "Z"), MakeShareable( new FFloatCurveKeyArea ( &TransformSection->GetScaleCurve( EAxis::Z ), TransformSection ) ) );
		LayoutBuilder.PopCategory();
	}

	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override 
	{
		// Add a box for the section
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect
		); 

		return LayerId;
	}

private:
	/** The section we are visualizing */
	UMovieSceneSection& Section;
};

F3DTransformTrackEditor::F3DTransformTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{
	// Listen for actor/component movement
	GEditor->OnBeginObjectMovement().AddRaw( this, &F3DTransformTrackEditor::OnPreTransformChanged );
	GEditor->OnEndObjectMovement().AddRaw( this, &F3DTransformTrackEditor::OnTransformChanged );
}

F3DTransformTrackEditor::~F3DTransformTrackEditor()
{
	GEditor->OnBeginObjectMovement().RemoveAll( this );
	GEditor->OnEndObjectMovement().RemoveAll( this );
}

TSharedRef<FMovieSceneTrackEditor> F3DTransformTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DTransformTrackEditor( InSequencer ) );
}

bool F3DTransformTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DTransformTrack::StaticClass();
}

TSharedRef<ISequencerSection> F3DTransformTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack* Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	TSharedRef<ISequencerSection> NewSection( new F3DTransformSection( SectionObject ) );

	return NewSection;
}


void F3DTransformTrackEditor::OnPreTransformChanged( UObject& InObject )
{
	UMovieScene* MovieScene = GetMovieScene();
	float AutoKeyTime = GetTimeForKey( MovieScene );

	if( IsAllowedToAutoKey() )
	{
		USceneComponent* SceneComponent = NULL;
		AActor* Actor = Cast<AActor>( &InObject );
		if( Actor && Actor->GetRootComponent() )
		{
			SceneComponent = Actor->GetRootComponent();
		}
		else
		{
			// If the object wasn't an actor attempt to get it directly as a scene component 
			SceneComponent = Cast<USceneComponent>( &InObject );
		}

		if( SceneComponent )
		{
			// Cache off the existing transform so we can detect which components have changed
			// and keys only when something has changed
			FTransformData Transform( SceneComponent );

			ObjectToExistingTransform.Add( &InObject, Transform );
		}
	}
}

/**
 * Temp struct used because delegates only accept 4 or less payloads
 * FTransformKey is immutable and would require heavy re-architecting to fit here
 */
struct FTransformDataPair
{
	FTransformDataPair(FTransformData InTransformData, FTransformData InLastTransformData)
		: TransformData(InTransformData)
		, LastTransformData(InLastTransformData) {}

	FTransformData TransformData;
	FTransformData LastTransformData;
};

void F3DTransformTrackEditor::OnTransformChanged( UObject& InObject )
{
	const TSharedPtr<ISequencer> Sequencer = GetSequencer();

	USceneComponent* SceneComponentThatChanged = NULL;

	// The runtime binding
	FGuid ObjectHandle;

	AActor* Actor = Cast<AActor>( &InObject );
	if( Actor && Actor->GetRootComponent() )
	{
		// Get a handle bound to the key/section we are adding so we know what objects to change during playback
		ObjectHandle = Sequencer->GetHandleToObject( Actor );
		SceneComponentThatChanged = Actor->GetRootComponent();
	}

	else
	{
		// If the object wasn't an actor attempt to get it directly as a scene component 
		SceneComponentThatChanged = Cast<USceneComponent>( &InObject );
		if( SceneComponentThatChanged )
		{
			ObjectHandle = Sequencer->GetHandleToObject( SceneComponentThatChanged );
		}

	}

	if( SceneComponentThatChanged && ObjectHandle.IsValid() )
	{
		// Find an existing transform if possible.  If one exists we will compare against the new one to decide what components of the transform need keys
		FTransformData ExistingTransform = ObjectToExistingTransform.FindRef( &InObject );

		// Remove it from the list of cached transforms. 
		// @todo sequencer livecapture: This can be made much for efficient by not removing cached state during live capture situation
		ObjectToExistingTransform.Remove( &InObject );

		// Build new transform data
		FTransformData NewTransformData( SceneComponentThatChanged );

		FTransformDataPair TransformPair(NewTransformData, ExistingTransform);

		AnimatablePropertyChanged(UMovieScene3DTransformTrack::StaticClass(), true,
			FOnKeyProperty::CreateRaw(this, &F3DTransformTrackEditor::OnTransformChangedInternals, &InObject, ObjectHandle, TransformPair, true));
	}
}

void F3DTransformTrackEditor::AddKey(const FGuid& ObjectGuid, UObject* AdditionalAsset)
{
	TArray<UObject*> OutObjects;
	GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneInstance(), ObjectGuid, OutObjects);

	for ( UObject* Object : OutObjects )
	{
		FGuid ObjectHandle;
		USceneComponent* SceneComponent = NULL;
		AActor* Actor = Cast<AActor>( Object );
		if( Actor && Actor->GetRootComponent() )
		{
			ObjectHandle = GetSequencer()->GetHandleToObject( Actor );
			SceneComponent = Actor->GetRootComponent();
		}
		else
		{
			// If the object wasn't an actor attempt to get it directly as a scene component 
			SceneComponent = Cast<USceneComponent>( Object );
			if( SceneComponent )
			{
				ObjectHandle = GetSequencer()->GetHandleToObject( SceneComponent );
			}
		}

		if( SceneComponent )
		{
			// Cache off the existing transform so we can detect which components have changed
			// and keys only when something has changed
			FTransformData CurrentTransform( SceneComponent );

			FTransformDataPair TransformPair(CurrentTransform, FTransformData());

			AnimatablePropertyChanged(UMovieScene3DTransformTrack::StaticClass(), false,
				FOnKeyProperty::CreateRaw(this, &F3DTransformTrackEditor::OnTransformChangedInternals, Object, ObjectHandle, TransformPair, false));
		}
	}
}

void F3DTransformTrackEditor::OnTransformChangedInternals(float KeyTime, UObject* InObject, FGuid ObjectHandle, FTransformDataPair TransformPair, bool bAutoKeying)
{
	// Only unwind rotation if we're generating keys while recording (scene is actively playing back)
	const bool bUnwindRotation = GetSequencer()->IsRecordingLive();

	FName Transform("Transform");
	if (ObjectHandle.IsValid())
	{
		UMovieSceneTrack* Track = GetTrackForObject( ObjectHandle, UMovieScene3DTransformTrack::StaticClass(), Transform );
		UMovieScene3DTransformTrack* TransformTrack = CastChecked<UMovieScene3DTransformTrack>( Track );
		// Transform name and path are the same
		TransformTrack->SetPropertyNameAndPath( Transform, Transform.ToString() );
	
		if (!TransformPair.LastTransformData.IsValid())
		{
			bool bHasTranslationKeys = false, bHasRotationKeys = false, bHasScaleKeys = false;
			TransformPair.LastTransformData.bValid = TransformTrack->Eval(KeyTime, KeyTime, TransformPair.LastTransformData.Translation, TransformPair.LastTransformData.Rotation, TransformPair.LastTransformData.Scale, bHasTranslationKeys, bHasRotationKeys, bHasScaleKeys);
		}

		FTransformKey TransformKey = FTransformKey(KeyTime, TransformPair.TransformData, TransformPair.LastTransformData);

		bool bSuccessfulAdd = TransformTrack->AddKeyToSection( ObjectHandle, TransformKey, bUnwindRotation );
		if (bSuccessfulAdd)
		{
			TransformTrack->SetAsShowable();
		}
	}
}
