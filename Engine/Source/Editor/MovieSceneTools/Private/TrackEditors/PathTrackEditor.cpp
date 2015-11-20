// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieScene3DPathTrack.h"
#include "MovieScene3DPathSection.h"
#include "ISequencerSection.h"
#include "ISectionLayoutBuilder.h"
#include "IKeyArea.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneTrackEditor.h"
#include "PathTrackEditor.h"
#include "ActorEditorUtils.h"
#include "Components/SplineComponent.h"
#include "ActorPickerTrackEditor.h"


#define LOCTEXT_NAMESPACE "FPathTrackEditor"

/**
 * Class that draws a path section in the sequencer
 */
class F3DPathSection
	: public ISequencerSection
{
public:
	F3DPathSection( UMovieSceneSection& InSection, F3DPathTrackEditor* InPathTrackEditor )
		: Section( InSection )
		, PathTrackEditor(InPathTrackEditor)
	{ }

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}

	virtual FText GetDisplayName() const override
	{ 
		return LOCTEXT("DisplayName", "Path");
	}
	
	virtual FText GetSectionTitle() const override { return FText::GetEmpty(); }

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		UMovieScene3DPathSection* PathSection = Cast<UMovieScene3DPathSection>( &Section );

		LayoutBuilder.AddKeyArea("Timing", LOCTEXT("TimingArea", "Timing"), MakeShareable( new FFloatCurveKeyArea ( &PathSection->GetTimingCurve( ), PathSection ) ) );
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
	
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder) override
	{
		FGuid DummyObjectBinding;

		MenuBuilder.AddSubMenu(
			LOCTEXT("SetPath", "Path"), LOCTEXT("SetPathTooltip", "Set path"),
			FNewMenuDelegate::CreateRaw(PathTrackEditor, &FActorPickerTrackEditor::ShowActorSubMenu, DummyObjectBinding, &Section));
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;

	/** The path track editor */
	F3DPathTrackEditor* PathTrackEditor;
};


F3DPathTrackEditor::F3DPathTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FActorPickerTrackEditor( InSequencer ) 
{ 
}


TSharedRef<ISequencerTrackEditor> F3DPathTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DPathTrackEditor( InSequencer ) );
}


bool F3DPathTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DPathTrack::StaticClass();
}


TSharedRef<ISequencerSection> F3DPathTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	return MakeShareable( new F3DPathSection( SectionObject, this ) );
}


void F3DPathTrackEditor::AddKey( const FGuid& ObjectGuid, UObject* AdditionalAsset )
{
	AActor* ParentActor = Cast<AActor>(AdditionalAsset);
	if (ParentActor != nullptr)
	{
		UMovieSceneSection* DummySection = nullptr;
		ActorPicked(ParentActor, ObjectGuid, DummySection);
	}
}


void F3DPathTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		UMovieSceneSection* DummySection = nullptr;

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddPath", "Path"), LOCTEXT("AddPathTooltip", "Adds a path track."),
			FNewMenuDelegate::CreateRaw(this, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, DummySection));
	}
}

bool F3DPathTrackEditor::IsActorPickable(const AActor* const ParentActor)
{
	if (ParentActor->IsListedInSceneOutliner() &&
		!FActorEditorUtils::IsABuilderBrush(ParentActor) &&
		!ParentActor->IsA( AWorldSettings::StaticClass() ) &&
		!ParentActor->IsPendingKill())
	{			
		TArray<USplineComponent*> SplineComponents;
		ParentActor->GetComponents(SplineComponents);
		if (SplineComponents.Num())
		{
			return true;
		}
	}
	return false;
}


void F3DPathTrackEditor::ActorSocketPicked(const FName SocketName, AActor* ParentActor, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	if (ObjectGuid.IsValid())
	{
		TArray<UObject*> OutObjects;
		GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectGuid, OutObjects);

		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &F3DPathTrackEditor::AddKeyInternal, OutObjects, ParentActor) );
	}
	else if (Section != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoSetPath", "Set Path"));

		UMovieScene3DPathSection* PathSection = (UMovieScene3DPathSection*)(Section);
		FGuid SplineId = FindOrCreateHandleToObject(ParentActor).Handle;

		if (SplineId.IsValid())
		{
			PathSection->SetConstraintId(SplineId);
		}
	}
}

bool F3DPathTrackEditor::AddKeyInternal( float KeyTime, const TArray<UObject*> Objects, AActor* ParentActor)
{
	bool bHandleCreated = false;
	bool bTrackCreated = false;
	bool bTrackModified = false;

	AActor* ActorWithSplineComponent = ParentActor;
	
	FGuid SplineId;

	if (ActorWithSplineComponent != nullptr)
	{
		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( ActorWithSplineComponent );
		SplineId = HandleResult.Handle;
		bHandleCreated |= HandleResult.bWasCreated;
	}

	if (!SplineId.IsValid())
	{
		return false;
	}

	for( int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex )
	{
		UObject* Object = Objects[ObjectIndex];

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object );
		FGuid ObjectHandle = HandleResult.Handle;
		bHandleCreated |= HandleResult.bWasCreated;
		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieScene3DPathTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				// Clamp to next path section's start time or the end of the current sequencer view range
				float PathEndTime = GetSequencer()->GetViewRange().GetUpperBoundValue();
	
				for (int32 PathSectionIndex = 0; PathSectionIndex < Track->GetAllSections().Num(); ++PathSectionIndex)
				{
					float StartTime = Track->GetAllSections()[PathSectionIndex]->GetStartTime();
					float EndTime = Track->GetAllSections()[PathSectionIndex]->GetEndTime();
					if (KeyTime < StartTime)
					{
						if (PathEndTime > StartTime)
						{
							PathEndTime = StartTime;
						}
					}
				}

				Cast<UMovieScene3DPathTrack>(Track)->AddConstraint( KeyTime, PathEndTime, NAME_None, SplineId );
				bTrackModified = true;
			}
		}
	}

	return bHandleCreated || bTrackCreated || bTrackModified;
}


#undef LOCTEXT_NAMESPACE
