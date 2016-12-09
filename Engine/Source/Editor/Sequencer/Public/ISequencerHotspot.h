// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"

class FMenuBuilder;
class ISequencer;
class ISequencerEditToolDragOperation;

enum class ESequencerHotspot
{
	Key,
	Section,
	SectionResize_L,
	SectionResize_R,
};


/** A sequencer hotspot is used to identify specific areas on the sequencer track area */ 
struct ISequencerHotspot
{
	virtual ~ISequencerHotspot() { }
	virtual ESequencerHotspot GetType() const = 0;
	virtual TSharedPtr<ISequencerEditToolDragOperation> InitiateDrag(ISequencer&) = 0;
	virtual void PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime){}
	virtual FCursorReply GetCursor() const { return FCursorReply::Unhandled(); }
};
