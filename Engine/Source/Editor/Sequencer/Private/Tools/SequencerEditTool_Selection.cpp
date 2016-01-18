// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerEntityVisitor.h"
#include "SequencerEditTool_Selection.h"
#include "MovieSceneSection.h"
#include "ISequencerSection.h"
#include "VirtualTrackArea.h"
#include "Sequencer.h"
#include "SSequencerTreeView.h"
#include "SequencerHotspots.h"


struct FSelectionPreviewVisitor
	: ISequencerEntityVisitor
{
	FSelectionPreviewVisitor(FSequencerSelectionPreview& InSelectionPreview, FSequencerSelection& InSelection, ESelectionPreviewState InSetStateTo)
		: SelectionPreview(InSelectionPreview)
		, ExistingSelection(InSelection)
		, SetStateTo(InSetStateTo)
	{}

	virtual void VisitKey(FKeyHandle KeyHandle, float KeyTime, const TSharedPtr<IKeyArea>& KeyArea, UMovieSceneSection* Section) const override
	{
		FSequencerSelectedKey Key(*Section, KeyArea, KeyHandle);

		bool bResetSectionSelection = false;

		// If we're trying to change this key's selection state, we reset the selection state of the section
		bool bKeyIsSelected = ExistingSelection.IsSelected(Key);
		if ( (bKeyIsSelected && SetStateTo == ESelectionPreviewState::NotSelected) ||
			(!bKeyIsSelected && SetStateTo == ESelectionPreviewState::Selected) )
		{
			SelectionPreview.SetSelectionState(Section, ESelectionPreviewState::Undefined);
			SectionsWithKeysSelected.Add(Section);
		}

		SelectionPreview.SetSelectionState(Key, SetStateTo);
	}

	virtual void VisitSection(UMovieSceneSection* Section) const
	{
		if (!SectionsWithKeysSelected.Contains(Section))
		{
			SelectionPreview.SetSelectionState(Section, SetStateTo);
		}
	}

private:
	mutable TSet<UMovieSceneSection*> SectionsWithKeysSelected;

	FSequencerSelectionPreview& SelectionPreview;
	FSequencerSelection& ExistingSelection;
	ESelectionPreviewState SetStateTo;
};


class FMarqueeDragOperation
	: public ISequencerEditToolDragOperation
{
public:

	FMarqueeDragOperation(TWeakPtr<FSequencer> InSequencer, TWeakPtr<SSequencer> InSequencerWidget)
		: Sequencer(MoveTemp(InSequencer))
		, SequencerWidget(MoveTemp(InSequencerWidget))
		, PreviewState(ESelectionPreviewState::Selected)
	{}

public:

	// ISequencerEditToolDragOperation interface

	virtual FCursorReply GetCursor() const override
	{
		return FCursorReply::Cursor( EMouseCursor::Default );
	}

	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override
	{
		// Start a new marquee selection
		InitialPosition =  VirtualTrackArea.PhysicalToVirtual(LocalMousePos);
		CurrentMousePos = LocalMousePos;

		if (MouseEvent.IsShiftDown())
		{
			PreviewState = ESelectionPreviewState::Selected;
		}
		else if (MouseEvent.IsAltDown())
		{
			PreviewState = ESelectionPreviewState::NotSelected;
		}
		else
		{
			PreviewState = ESelectionPreviewState::Selected;

			// @todo: selection in transactions
			Sequencer.Pin()->GetSelection().Empty();
		}
	}

	virtual void OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override
	{
		// hange the current marquee selection
		const FVector2D MouseDelta = MouseEvent.GetCursorDelta();
		auto PinnedSequencer = Sequencer.Pin();

		// Handle virtual scrolling when at the vertical extremes of the widget (performed before we clamp the mouse pos)
		{
			const float ScrollThresholdV = VirtualTrackArea.GetPhysicalSize().Y * 0.025f;

			float Difference = LocalMousePos.Y - ScrollThresholdV;
			if (Difference < 0 && MouseDelta.Y < 0)
			{
				PinnedSequencer->VerticalScroll( Difference * 0.1f );
			}

			Difference = LocalMousePos.Y - (VirtualTrackArea.GetPhysicalSize().Y - ScrollThresholdV);
			if (Difference > 0 && MouseDelta.Y > 0)
			{
				PinnedSequencer->VerticalScroll( Difference * 0.1f );
			}
		}

		// Clamp the vertical position to the actual bounds of the track area
		LocalMousePos.Y = FMath::Clamp(LocalMousePos.Y, 0.f, VirtualTrackArea.GetPhysicalSize().Y);
		CurrentPosition = VirtualTrackArea.PhysicalToVirtual(LocalMousePos);

		// Clamp software cursor position to bounds of the track area
		CurrentMousePos = LocalMousePos;
		CurrentMousePos.X = FMath::Clamp(CurrentMousePos.X, 0.f, VirtualTrackArea.GetPhysicalSize().X);

		TRange<float> ViewRange = PinnedSequencer->GetViewRange();

		// Handle virtual scrolling when at the horizontal extremes of the widget
		{
			const float ScrollThresholdH = ViewRange.Size<float>() * 0.025f;

			float Difference = CurrentPosition.X - (ViewRange.GetLowerBoundValue() + ScrollThresholdH);
			if (Difference < 0 && MouseDelta.X < 0)
			{
				PinnedSequencer->StartAutoscroll(Difference);
			}
			else
			{
				Difference = CurrentPosition.X - (ViewRange.GetUpperBoundValue() - ScrollThresholdH);
				if (Difference > 0 && MouseDelta.X > 0)
				{
					PinnedSequencer->StartAutoscroll(Difference);
				}
				else
				{
					PinnedSequencer->StopAutoscroll();
				}
			}
		}

		// Calculate the size of a key in virtual space
		FVector2D VirtualKeySize;
		VirtualKeySize.X = SequencerSectionConstants::KeySize.X / VirtualTrackArea.GetPhysicalSize().X * ViewRange.Size<float>();
		// Vertically, virtual units == physical units
		VirtualKeySize.Y = SequencerSectionConstants::KeySize.Y;

		// Visit everything using the preview selection primarily as well as the 
		auto& SelectionPreview = PinnedSequencer->GetSelectionPreview();

		// Ensure the preview is empty before calculating the intersection
		SelectionPreview.Empty();

		const auto& RootNodes = SequencerWidget.Pin()->GetTreeView()->GetNodeTree()->GetRootNodes();

		// Now walk everything within the current marquee range, setting preview selection states as we go
		FSequencerEntityWalker Walker(FSequencerEntityRange(TopLeft(), BottomRight()), VirtualKeySize);
		Walker.Traverse(FSelectionPreviewVisitor(SelectionPreview, PinnedSequencer->GetSelection(), PreviewState), RootNodes);
	}

	virtual void OnEndDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) override
	{
		// finish dragging the marquee selection
		auto PinnedSequencer = Sequencer.Pin();
		auto& Selection = PinnedSequencer->GetSelection();
		auto& SelectionPreview = PinnedSequencer->GetSelectionPreview();

		// Patch everything from the selection preview into the actual selection
		for (const auto& Pair : SelectionPreview.GetDefinedKeyStates())
		{
			if (Pair.Value == ESelectionPreviewState::Selected)
			{
				// Select it in the main selection
				Selection.AddToSelection(Pair.Key);
			}
			else
			{
				Selection.RemoveFromSelection(Pair.Key);
			}
		}

		for (const auto& Pair : SelectionPreview.GetDefinedSectionStates())
		{
			UMovieSceneSection* Section = Pair.Key.Get();
			if (Pair.Value == ESelectionPreviewState::Selected)
			{
				// Select it in the main selection
				Selection.AddToSelection(Section);
			}
			else
			{
				Selection.RemoveFromSelection(Section);
			}
		}

		// We're done with this now
		SelectionPreview.Empty();
	}

	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
	{
		// convert to physical space for rendering
		const FVirtualTrackArea VirtualTrackArea = SequencerWidget.Pin()->GetVirtualTrackArea();

		FVector2D SelectionTopLeft = VirtualTrackArea.VirtualToPhysical(TopLeft());
		FVector2D SelectionBottomRight = VirtualTrackArea.VirtualToPhysical(BottomRight());

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(SelectionTopLeft, SelectionBottomRight - SelectionTopLeft),
			FEditorStyle::GetBrush(TEXT("MarqueeSelection")),
			MyClippingRect
			);

		return LayerId + 1;
	}

private:

	FVector2D TopLeft() const
	{
		return FVector2D(
			FMath::Min(InitialPosition.X, CurrentPosition.X),
			FMath::Min(InitialPosition.Y, CurrentPosition.Y)
		);
	}
	
	FVector2D BottomRight() const
	{
		return FVector2D(
			FMath::Max(InitialPosition.X, CurrentPosition.X),
			FMath::Max(InitialPosition.Y, CurrentPosition.Y)
		);
	}

	/** The sequencer itself */
	TWeakPtr<FSequencer> Sequencer;

	/** Sequencer widget */
	TWeakPtr<SSequencer> SequencerWidget;

	/** Whether we should select/deselect things in this marquee operation */
	ESelectionPreviewState PreviewState;

	FVector2D InitialPosition;
	FVector2D CurrentPosition;
	FVector2D CurrentMousePos;
};


FSequencerEditTool_Selection::FSequencerEditTool_Selection(TSharedPtr<FSequencer> InSequencer, TSharedPtr<SSequencer> InSequencerWidget)
	: Sequencer(InSequencer)
	, SequencerWidget(InSequencerWidget)
	, CursorDecorator(nullptr)
{ }


ISequencer& FSequencerEditTool_Selection::GetSequencer() const
{
	return *Sequencer.Pin();
}


FCursorReply FSequencerEditTool_Selection::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(EMouseCursor::Crosshairs);
}


int32 FSequencerEditTool_Selection::OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (DragOperation.IsValid())
	{
		LayerId = DragOperation->OnPaint(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId);
	}

	if (CursorDecorator)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(MousePosition + FVector2D(5, 5), CursorDecorator->ImageSize),
			CursorDecorator,
			MyClippingRect
			);
	}

	return LayerId;
}


FReply FSequencerEditTool_Selection::OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateCursor(MyGeometry, MouseEvent);

	DelayedDrag.Reset();

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		DelayedDrag = FDelayedDrag_Hotspot(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), EKeys::LeftMouseButton, Hotspot);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


FReply FSequencerEditTool_Selection::OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateCursor(MyGeometry, MouseEvent);

	if (DelayedDrag.IsSet())
	{
		FReply Reply = FReply::Handled();

		const FVirtualTrackArea VirtualTrackArea = SequencerWidget.Pin()->GetVirtualTrackArea();

		if (DragOperation.IsValid())
		{
			FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			DragOperation->OnDrag(MouseEvent, LocalPosition, VirtualTrackArea );
		}
		else if (DelayedDrag->AttemptDragStart(MouseEvent))
		{
			if (DelayedDrag->Hotspot.IsValid())
			{
				// We only allow resizing with the marquee selection tool enabled
				auto HotspotType = DelayedDrag->Hotspot->GetType();
				if (HotspotType != ESequencerHotspot::Section && HotspotType != ESequencerHotspot::Key)
				{
					DragOperation = DelayedDrag->Hotspot->InitiateDrag(*Sequencer.Pin());
				}
			}

			if (!DragOperation.IsValid())
			{
				DragOperation = MakeShareable( new FMarqueeDragOperation(Sequencer, SequencerWidget) );
			}

			if (DragOperation.IsValid())
			{
				FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				DragOperation->OnBeginDrag(MouseEvent, LocalPosition, VirtualTrackArea);

				// Steal the capture, as we're now the authoritative widget in charge of a mouse-drag operation
				Reply.CaptureMouse(OwnerWidget.AsShared());
			}
		}

		return Reply;
	}
	return FReply::Unhandled();
}


FReply FSequencerEditTool_Selection::OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateCursor(MyGeometry, MouseEvent);

	DelayedDrag.Reset();
	if (DragOperation.IsValid())
	{
		FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		DragOperation->OnEndDrag(MouseEvent, LocalPosition, SequencerWidget.Pin()->GetVirtualTrackArea() );
		DragOperation = nullptr;

		CursorDecorator = nullptr;

		Sequencer.Pin()->StopAutoscroll();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FSequencerEditTool_Default::OnMouseButtonUp(OwnerWidget, MyGeometry, MouseEvent);
}


void FSequencerEditTool_Selection::OnMouseLeave(SWidget& OwnerWidget, const FPointerEvent& MouseEvent)
{
	if (!DragOperation.IsValid())
	{
		CursorDecorator = nullptr;
	}
}


void FSequencerEditTool_Selection::OnMouseCaptureLost()
{
	DelayedDrag.Reset();
	DragOperation = nullptr;
	CursorDecorator = nullptr;
}


FName FSequencerEditTool_Selection::GetIdentifier() const
{
	static FName Identifier("Selection");
	return Identifier;
}


void FSequencerEditTool_Selection::UpdateCursor(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// Don't update the brush if we have a drag operation
	if (!DragOperation.IsValid())
	{
		if (MouseEvent.IsShiftDown())
		{
			CursorDecorator = FEditorStyle::Get().GetBrush(TEXT("Sequencer.CursorDecorator_MarqueeAdd"));
		}
		else if (MouseEvent.IsAltDown())
		{
			CursorDecorator = FEditorStyle::Get().GetBrush(TEXT("Sequencer.CursorDecorator_MarqueeSubtract"));
		}
		else
		{
			CursorDecorator = FEditorStyle::Get().GetBrush(TEXT("Sequencer.CursorDecorator_Marquee"));
		}
	}
}
