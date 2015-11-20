// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ScopedTransaction.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieScene3DAttachTrack.h"
#include "MovieScene3DAttachSection.h"
#include "ISequencerSection.h"
#include "ISectionLayoutBuilder.h"
#include "IKeyArea.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneTrackEditor.h"
#include "ActorEditorUtils.h"
#include "GameFramework/WorldSettings.h"
#include "AttachTrackEditor.h"
#include "ActorPickerTrackEditor.h"


#define LOCTEXT_NAMESPACE "F3DAttachTrackEditor"

/**
 * Class that draws an attach section in the sequencer
 */
class F3DAttachSection
	: public ISequencerSection
{
public:

	F3DAttachSection( UMovieSceneSection& InSection, F3DAttachTrackEditor* InAttachTrackEditor )
		: Section( InSection )
		, AttachTrackEditor(InAttachTrackEditor)
	{ }

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}

	virtual FText GetDisplayName() const override
	{ 
		return LOCTEXT("SectionDisplayName", "Attach");
	}
	
	virtual FText GetSectionTitle() const override { return FText::GetEmpty(); }

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override {}

	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override 
	{
		const ESlateDrawEffect::Type DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
		// Add a box for the section
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect,
			DrawEffects
		); 

		return LayerId;
	}
	
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder) override
	{
		FGuid DummyObjectBinding;

		MenuBuilder.AddSubMenu(
			LOCTEXT("SetAttach", "Attach"), LOCTEXT("SetAttachTooltip", "Set attach"),
			FNewMenuDelegate::CreateRaw(AttachTrackEditor, &FActorPickerTrackEditor::ShowActorSubMenu, DummyObjectBinding, &Section));
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;

	/** The attach track editor */
	F3DAttachTrackEditor* AttachTrackEditor;
};


F3DAttachTrackEditor::F3DAttachTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FActorPickerTrackEditor( InSequencer ) 
{
}


F3DAttachTrackEditor::~F3DAttachTrackEditor()
{
}


TSharedRef<ISequencerTrackEditor> F3DAttachTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DAttachTrackEditor( InSequencer ) );
}


bool F3DAttachTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DAttachTrack::StaticClass();
}


TSharedRef<ISequencerSection> F3DAttachTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	return MakeShareable( new F3DAttachSection( SectionObject, this ) );
}


void F3DAttachTrackEditor::AddKey( const FGuid& ObjectGuid, UObject* AdditionalAsset )
{
	AActor* ParentActor = Cast<AActor>(AdditionalAsset);
	if (ParentActor != nullptr)
	{
		UMovieSceneSection* DummySection = nullptr;
		ActorPicked(ParentActor, ObjectGuid, DummySection);
	}
}

void F3DAttachTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		UMovieSceneSection* DummySection = nullptr;

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddAttach", "Attach"), LOCTEXT("AddAttachTooltip", "Adds an attach track."),
			FNewMenuDelegate::CreateRaw(this, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, DummySection));
	}
}


bool F3DAttachTrackEditor::IsActorPickable(const AActor* const ParentActor)
{
	if (ParentActor->IsListedInSceneOutliner() &&
		!FActorEditorUtils::IsABuilderBrush(ParentActor) &&
		!ParentActor->IsA( AWorldSettings::StaticClass() ) &&
		!ParentActor->IsPendingKill())
	{			
		return true;
	}
	return false;
}


void F3DAttachTrackEditor::ActorSocketPicked(const FName SocketName, AActor* ParentActor, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	if (ObjectGuid.IsValid())
	{
		TArray<UObject*> OutObjects;
		GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneSequenceInstance(), ObjectGuid, OutObjects);

		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &F3DAttachTrackEditor::AddKeyInternal, OutObjects, SocketName, ParentActor) );
	}
	else if (Section != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoSetAttach", "Set Attach"));

		UMovieScene3DAttachSection* AttachSection = (UMovieScene3DAttachSection*)(Section);
		FGuid ActorId = FindOrCreateHandleToObject(ParentActor).Handle;

		if (ActorId.IsValid())
		{
			AttachSection->SetConstraintId(ActorId);
			AttachSection->AttachSocketName = SocketName;
		}
	}
}

bool F3DAttachTrackEditor::AddKeyInternal( float KeyTime, const TArray<UObject*> Objects, const FName SocketName, AActor* ParentActor)
{
	bool bHandleCreated = false;
	bool bTrackCreated = false;
	bool bTrackModified = false;

	FGuid ParentActorId;

	if (ParentActor != nullptr)
	{
		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(ParentActor);
		ParentActorId = HandleResult.Handle;
		bHandleCreated |= HandleResult.bWasCreated;
	}

	if (!ParentActorId.IsValid())
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
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject( ObjectHandle, UMovieScene3DAttachTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				// Clamp to next attach section's start time or the end of the current sequencer view range
				float AttachEndTime = GetSequencer()->GetViewRange().GetUpperBoundValue();

				for (int32 AttachSectionIndex = 0; AttachSectionIndex < Track->GetAllSections().Num(); ++AttachSectionIndex)
				{
					float StartTime = Track->GetAllSections()[AttachSectionIndex]->GetStartTime();
					float EndTime = Track->GetAllSections()[AttachSectionIndex]->GetEndTime();
					if (KeyTime < StartTime)
					{
						if (AttachEndTime > StartTime)
						{
							AttachEndTime = StartTime;
						}
					}
				}

				Cast<UMovieScene3DAttachTrack>(Track)->AddConstraint( KeyTime, AttachEndTime, SocketName, ParentActorId );
				bTrackModified = true;
			}
		}
	}

	return bHandleCreated || bTrackCreated || bTrackModified;
}


#undef LOCTEXT_NAMESPACE
