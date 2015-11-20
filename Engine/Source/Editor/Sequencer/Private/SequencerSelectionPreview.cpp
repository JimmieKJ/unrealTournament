// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerSelectionPreview.h"
#include "MovieSceneSection.h"

void FSequencerSelectionPreview::SetSelectionState(FSequencerSelectedKey Key, ESelectionPreviewState InState)
{
	if (InState == ESelectionPreviewState::Undefined)
	{
		DefinedKeyStates.Remove(Key);
	}
	else
	{
		DefinedKeyStates.Add(Key, InState);
	}
}

void FSequencerSelectionPreview::SetSelectionState(UMovieSceneSection* Section, ESelectionPreviewState InState)
{
	if (InState == ESelectionPreviewState::Undefined)
	{
		DefinedSectionStates.Remove(Section);
	}
	else
	{
		DefinedSectionStates.Add(Section, InState);
	}
}

ESelectionPreviewState FSequencerSelectionPreview::GetSelectionState(FSequencerSelectedKey Key) const
{
	if (auto* State = DefinedKeyStates.Find(Key))
	{
		return *State;
	}
	return ESelectionPreviewState::Undefined;
}

ESelectionPreviewState FSequencerSelectionPreview::GetSelectionState(UMovieSceneSection* Section) const
{
	if (auto* State = DefinedSectionStates.Find(Section))
	{
		return *State;
	}
	return ESelectionPreviewState::Undefined;
}

void FSequencerSelectionPreview::Empty()
{
	EmptyDefinedKeyStates();
	EmptyDefinedSectionStates();
}

void FSequencerSelectionPreview::EmptyDefinedKeyStates()
{
	DefinedKeyStates.Reset();
}

void FSequencerSelectionPreview::EmptyDefinedSectionStates()
{
	DefinedSectionStates.Reset();
}