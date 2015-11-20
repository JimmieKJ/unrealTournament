// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerEditTool_Movement.h"
#include "Sequencer.h"
#include "VirtualTrackArea.h"
#include "SequencerHotspots.h"
#include "EditToolDragOperations.h"


FSequencerEditTool_Movement::FSequencerEditTool_Movement(TSharedPtr<FSequencer> InSequencer, TSharedPtr<SSequencer> InSequencerWidget)
	: Sequencer(InSequencer)
	, SequencerWidget(InSequencerWidget)
{ }


ISequencer& FSequencerEditTool_Movement::GetSequencer() const
{
	return *Sequencer.Pin();
}


FReply FSequencerEditTool_Movement::OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DelayedDrag.Reset();

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		const FVirtualTrackArea VirtualTrackArea = SequencerWidget.Pin()->GetVirtualTrackArea();

		DelayedDrag = FDelayedDrag_Hotspot(VirtualTrackArea.CachedTrackAreaGeometry().AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), MouseEvent.GetEffectingButton(), Hotspot);

		if (Sequencer.Pin()->GetSettings()->GetSnapPlayTimeToDraggedKey())
		{
			if (DelayedDrag->Hotspot.IsValid())
			{
				if (DelayedDrag->Hotspot->GetType() == ESequencerHotspot::Key)
				{
					FSequencerSelectedKey& ThisKey = StaticCastSharedPtr<FKeyHotspot>(DelayedDrag->Hotspot)->Key;
					Sequencer.Pin()->SetGlobalTime(ThisKey.KeyArea->GetKeyTime(ThisKey.KeyHandle.GetValue()));
				}
			}
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


FReply FSequencerEditTool_Movement::OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DelayedDrag.IsSet())
	{
		const FVirtualTrackArea VirtualTrackArea = SequencerWidget.Pin()->GetVirtualTrackArea();

		FReply Reply = FReply::Handled();

		if (DelayedDrag->IsDragging())
		{
			// If we're already dragging, just update the drag op if it exists
			if (DragOperation.IsValid())
			{
				DragOperation->OnDrag(MouseEvent, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), VirtualTrackArea);
			}
		}
		// Otherwise we can attempt a new drag
		else if (DelayedDrag->AttemptDragStart(MouseEvent))
		{
			DragOperation = CreateDrag();

			if (DragOperation.IsValid())
			{
				DragOperation->OnBeginDrag(MouseEvent, DelayedDrag->GetInitialPosition(), VirtualTrackArea);

				// Steal the capture, as we're now the authoritative widget in charge of a mouse-drag operation
				Reply.CaptureMouse(OwnerWidget.AsShared());
			}
		}

		return Reply;
	}
	return FReply::Unhandled();
}


TSharedPtr<ISequencerEditToolDragOperation> FSequencerEditTool_Movement::CreateDrag()
{
	auto PinnedSequencer = Sequencer.Pin();
	FSequencerSelection& Selection = PinnedSequencer->GetSelection();

	if (DelayedDrag->Hotspot.IsValid())
	{
		// Let the hotspot start a drag first, if it wants to
		auto HotspotDrag = DelayedDrag->Hotspot->InitiateDrag(*PinnedSequencer);
		if (HotspotDrag.IsValid())
		{
			return HotspotDrag;
		}

		// Ok, the hotspot doesn't know how to drag - let's decide for ourselves
		auto HotspotType = DelayedDrag->Hotspot->GetType();

		// Moving section(s)?
		if (HotspotType == ESequencerHotspot::Section)
		{
			TArray<FSectionHandle> SectionHandles;

			FSectionHandle& ThisHandle = StaticCastSharedPtr<FSectionHotspot>(DelayedDrag->Hotspot)->Section;
			UMovieSceneSection* ThisSection = ThisHandle.GetSectionObject();
			if (Selection.IsSelected(ThisSection))
			{
				SectionHandles = SequencerWidget.Pin()->GetSectionHandles(Selection.GetSelectedSections());
			}
			else
			{
				Selection.EmptySelectedKeys();
				Selection.EmptySelectedSections();
				Selection.AddToSelection(ThisSection);
				SectionHandles.Add(ThisHandle);
			}
			return MakeShareable( new FMoveSection( *PinnedSequencer, SectionHandles ) );
		}
		// Moving key(s)?
		else if (HotspotType == ESequencerHotspot::Key)
		{
			FSequencerSelectedKey& ThisKey = StaticCastSharedPtr<FKeyHotspot>(DelayedDrag->Hotspot)->Key;

			// If it's not selected, we'll treat this as a unique drag
			if (!Selection.IsSelected(ThisKey))
			{
				Selection.EmptySelectedKeys();
				Selection.EmptySelectedSections();
				Selection.AddToSelection(ThisKey);
			}

			return MakeShareable( new FMoveKeys( *PinnedSequencer, Selection.GetSelectedKeys() ) );
		}
	}
	// If we're not dragging a hotspot, sections take precedence over keys
	else if (Selection.GetSelectedSections().Num())
	{
		return MakeShareable( new FMoveSection( *PinnedSequencer, SequencerWidget.Pin()->GetSectionHandles(Selection.GetSelectedSections()) ) );
	}
	else if (Selection.GetSelectedKeys().Num())
	{
		return MakeShareable( new FMoveKeys( *PinnedSequencer, Selection.GetSelectedKeys() ) );
	}

	return nullptr;
}


FReply FSequencerEditTool_Movement::OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DelayedDrag.Reset();

	if (DragOperation.IsValid())
	{
		DragOperation->OnEndDrag(MouseEvent, MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), SequencerWidget.Pin()->GetVirtualTrackArea());
		DragOperation = nullptr;

		// Only return handled if we actually started a drag
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FSequencerEditTool_Default::OnMouseButtonUp(OwnerWidget, MyGeometry, MouseEvent);
}


void FSequencerEditTool_Movement::OnMouseCaptureLost()
{
	DelayedDrag.Reset();
	DragOperation = nullptr;
}


FCursorReply FSequencerEditTool_Movement::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return DragOperation.IsValid() ? FCursorReply::Cursor(EMouseCursor::CardinalCross) : FCursorReply::Cursor(EMouseCursor::Default);
}


FName FSequencerEditTool_Movement::GetIdentifier() const
{
	static FName Identifier("Movement");
	return Identifier;
}
