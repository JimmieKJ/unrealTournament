// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerDetailKeyframeHandler.h"
#include "PropertyHandle.h"

FSequencerDetailKeyframeHandler::FSequencerDetailKeyframeHandler(TSharedRef<ISequencer> InSequencer) 
	: Sequencer(InSequencer)
{}

bool FSequencerDetailKeyframeHandler::IsPropertyKeyable(UClass* InObjectClass, const IPropertyHandle& InPropertyHandle) const
{
	return Sequencer.Pin()->CanKeyProperty(FCanKeyPropertyParams(InObjectClass, InPropertyHandle));
}

bool FSequencerDetailKeyframeHandler::IsPropertyKeyingEnabled() const
{
	return Sequencer.IsValid() && Sequencer.Pin()->GetFocusedMovieSceneSequence();
}

void FSequencerDetailKeyframeHandler::OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle)
{
	TArray<UObject*> Objects;
	KeyedPropertyHandle.GetOuterObjects( Objects );

	FKeyPropertyParams KeyPropertyParams(Objects, KeyedPropertyHandle, ESequencerKeyMode::ManualKeyForced);

	Sequencer.Pin()->KeyProperty(KeyPropertyParams);
}
