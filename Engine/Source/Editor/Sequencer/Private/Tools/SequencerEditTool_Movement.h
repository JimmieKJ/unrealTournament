// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerEditTool_Default.h"
#include "DelayedDrag.h"


class ISequencerEditToolDragOperation;


class FSequencerEditTool_Movement
	: public FSequencerEditTool_Default
{
public:

	/** Create and initialize a new instance. */
	FSequencerEditTool_Movement(TSharedPtr<FSequencer> InSequencer, TSharedPtr<SSequencer> InSequencerWidget);

public:

	// ISequencerEditTool interface

	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost() override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FName GetIdentifier() const override;
	virtual ISequencer& GetSequencer() const override;

private:

	TSharedPtr<ISequencerEditToolDragOperation> CreateDrag();

	/** The sequencer itself */
	TWeakPtr<FSequencer> Sequencer;

	/** Sequencer widget */
	TWeakPtr<SSequencer> SequencerWidget;

	struct FDelayedDrag_Hotspot : FDelayedDrag
	{
		FDelayedDrag_Hotspot(FVector2D InInitialPosition, FKey InApplicableKey, TSharedPtr<ISequencerHotspot> InHotspot)
			: FDelayedDrag(InInitialPosition, InApplicableKey)
			, Hotspot(MoveTemp(InHotspot))
		{ }

		TSharedPtr<ISequencerHotspot> Hotspot;
	};

	/** Helper class responsible for handling delayed dragging */
	TOptional<FDelayedDrag_Hotspot> DelayedDrag;

	/** Current drag operation if any */
	TSharedPtr<ISequencerEditToolDragOperation> DragOperation;
};
