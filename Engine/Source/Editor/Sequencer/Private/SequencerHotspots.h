// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "SequencerSelectedKey.h"
#include "ISequencerHotspot.h"
#include "DisplayNodes/SequencerTrackNode.h"

class FMenuBuilder;
class ISequencer;
class ISequencerEditToolDragOperation;

/** A hotspot representing a key */
struct FKeyHotspot
	: ISequencerHotspot
{
	FKeyHotspot(FSequencerSelectedKey InKey)
		: Key(InKey)
	{ }

	virtual ESequencerHotspot GetType() const override { return ESequencerHotspot::Key; }
	virtual TSharedPtr<ISequencerEditToolDragOperation> InitiateDrag(ISequencer&) override { return nullptr; }
	virtual void PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime) override;

	/** The key itself */
	FSequencerSelectedKey Key;
};


/** Structure used to encapsulate a section and its track node */
struct FSectionHandle
{
	FSectionHandle(TSharedPtr<FSequencerTrackNode> InTrackNode, int32 InSectionIndex)
		: SectionIndex(InSectionIndex), TrackNode(MoveTemp(InTrackNode))
	{ }

	friend bool operator==(const FSectionHandle& A, const FSectionHandle& B)
	{
		return A.SectionIndex == B.SectionIndex && A.TrackNode == B.TrackNode;
	}
	
	UMovieSceneSection* GetSectionObject() const { return TrackNode->GetSections()[SectionIndex]->GetSectionObject(); }

	int32 SectionIndex;
	TSharedPtr<FSequencerTrackNode> TrackNode;
};


/** A hotspot representing a section */
struct FSectionHotspot
	: ISequencerHotspot
{
	FSectionHotspot(FSectionHandle InSection)
		: Section(InSection)
	{ }

	virtual ESequencerHotspot GetType() const override { return ESequencerHotspot::Section; }
	virtual TSharedPtr<ISequencerEditToolDragOperation> InitiateDrag(ISequencer&) override { return nullptr; }
	virtual void PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime) override;

	/** Handle to the section */
	FSectionHandle Section;
};


/** A hotspot representing a resize handle on a section */
struct FSectionResizeHotspot
	: ISequencerHotspot
{
	enum EHandle
	{
		Left,
		Right
	};

	FSectionResizeHotspot(EHandle InHandleType, FSectionHandle InSection) : Section(InSection), HandleType(InHandleType) {}

	virtual ESequencerHotspot GetType() const override { return HandleType == Left ? ESequencerHotspot::SectionResize_L : ESequencerHotspot::SectionResize_R; }
	virtual TSharedPtr<ISequencerEditToolDragOperation> InitiateDrag(ISequencer& Sequencer) override;
	virtual FCursorReply GetCursor() const { return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight ); }

	/** Handle to the section */
	FSectionHandle Section;

private:

	EHandle HandleType;
};
