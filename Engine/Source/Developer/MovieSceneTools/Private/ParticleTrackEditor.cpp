// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "MovieSceneTrack.h"
#include "MovieSceneParticleTrack.h"
#include "ScopedTransaction.h"
#include "ISequencerObjectChangeListener.h"
#include "ISectionLayoutBuilder.h"
#include "IKeyArea.h"
#include "MovieSceneToolHelpers.h"
#include "MovieSceneTrackEditor.h"
#include "ParticleTrackEditor.h"
#include "MovieSceneParticleSection.h"
#include "CommonMovieSceneTools.h"
#include "AssetRegistryModule.h"
#include "Particles/Emitter.h"


namespace AnimatableParticleEditorConstants
{
	// @todo Sequencer Allow this to be customizable
	const uint32 ParticleTrackHeight = 20;
}



FParticleSection::FParticleSection( UMovieSceneSection& InSection )
	: Section( InSection )
{
}

FParticleSection::~FParticleSection()
{
}

UMovieSceneSection* FParticleSection::GetSectionObject()
{ 
	return &Section;
}

FText FParticleSection::GetDisplayName() const
{
	return NSLOCTEXT("FParticleSection", "Emitter", "Emitter");
}

FText FParticleSection::GetSectionTitle() const
{
	EParticleKey::Type KeyType = Cast<UMovieSceneParticleSection>(&Section)->GetKeyType();
	return KeyType == EParticleKey::Toggle ? NSLOCTEXT("FParticleSection", "Toggle", "Toggle") :
		KeyType == EParticleKey::Trigger ? NSLOCTEXT("FParticleSection", "Trigger", "Trigger") : NSLOCTEXT("FParticleSection", "None", "None");
}

float FParticleSection::GetSectionHeight() const
{
	return (float)AnimatableParticleEditorConstants::ParticleTrackHeight;
}

int32 FParticleSection::OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const
{
	UMovieSceneParticleSection* AnimSection = Cast<UMovieSceneParticleSection>(&Section);
	
	FTimeToPixel TimeToPixelConverter( AllottedGeometry, TRange<float>( Section.GetStartTime(), Section.GetEndTime() ) );
	
	EParticleKey::Type KeyType = Cast<UMovieSceneParticleSection>(&Section)->GetKeyType();

	// @todo Sequencer Particle Triggers should be sections with 0 duration, but a custom graphic which
	// allows them to be dragged around
	if (KeyType == EParticleKey::Trigger)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect,
			ESlateDrawEffect::None,
			FLinearColor(0.6f, 0.4f, 0.3f, 1.f)
		);
	}
	else
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect,
			ESlateDrawEffect::None,
			FLinearColor(0.8f, 0.4f, 0.3f, 1.f)
		);
	}

	return LayerId+1;
}

bool FParticleSection::SectionIsResizable() const
{
	return Cast<UMovieSceneParticleSection>(&Section)->GetKeyType() != EParticleKey::Trigger;
}



FParticleTrackEditor::FParticleTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{
}

FParticleTrackEditor::~FParticleTrackEditor()
{
}

TSharedRef<FMovieSceneTrackEditor> FParticleTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FParticleTrackEditor( InSequencer ) );
}

bool FParticleTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneParticleTrack::StaticClass();
}

TSharedRef<ISequencerSection> FParticleTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack* Track )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	
	TSharedRef<ISequencerSection> NewSection( new FParticleSection(SectionObject) );

	return NewSection;
}

void FParticleTrackEditor::BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AEmitter::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "TriggerParticles", "Trigger"),
			NSLOCTEXT("Sequencer", "TriggerParticlesTooltip", "Adds a trigger event for this particle emitter at the current time."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FParticleTrackEditor::AddParticleKey, ObjectBinding, true))
			);
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "ToggleParticles", "Toggle"),
			NSLOCTEXT("Sequencer", "ToggleParticlesTooltip", "Adds a toggle section for this particle emitter at the current time."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FParticleTrackEditor::AddParticleKey, ObjectBinding, false))
			);
	}
}

void FParticleTrackEditor::AddParticleKey(const FGuid ObjectGuid, bool bTrigger)
{
	TArray<UObject*> OutObjects;
	GetSequencer()->GetRuntimeObjects( GetSequencer()->GetFocusedMovieSceneInstance(), ObjectGuid, OutObjects );

	AnimatablePropertyChanged( UMovieSceneParticleTrack::StaticClass(), false,
		FOnKeyProperty::CreateRaw( this, &FParticleTrackEditor::AddKeyInternal, OutObjects, bTrigger) );
}

void FParticleTrackEditor::AddKeyInternal( float KeyTime, const TArray<UObject*> Objects, bool bTrigger)
{
	for( int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex )
	{
		UObject* Object = Objects[ObjectIndex];

		FGuid ObjectHandle = FindOrCreateHandleToObject( Object );
		if (ObjectHandle.IsValid())
		{
			UMovieSceneTrack* Track = GetTrackForObject( ObjectHandle, UMovieSceneParticleTrack::StaticClass(), FName("ParticleSystem"));

			if (ensure(Track))
			{
				Cast<UMovieSceneParticleTrack>(Track)->AddNewParticleSystem( KeyTime, bTrigger );
			}
		}
	}
}
