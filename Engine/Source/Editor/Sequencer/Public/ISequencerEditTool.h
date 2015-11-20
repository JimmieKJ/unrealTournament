// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FVirtualTrackArea;
struct ISequencerHotspot;


/**
 * Interface for drag and drop operations that are handled by edit tools in Sequencer.
 */
class ISequencerEditToolDragOperation
{
public:

	/**
	 * Called to initiate a drag
	 *
	 * @param MouseEvent		The associated mouse event for dragging
	 * @param LocalMousePos		The current location of the mouse, relative to the top-left corner of the physical track area
	 * @param VirtualTrackArea	A virtual track area that can be used for pixel->time conversions and hittesting
	 */
	virtual void OnBeginDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) = 0;

	/**
	 * Notification called when the mouse moves while dragging
	 *
	 * @param MouseEvent		The associated mouse event for dragging
	 * @param LocalMousePos		The current location of the mouse, relative to the top-left corner of the physical track area
	 * @param VirtualTrackArea	A virtual track area that can be used for pixel->time conversions and hittesting
	 */
	virtual void OnDrag(const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) = 0;

	/** Called when a drag has ended
	 *
	 * @param MouseEvent		The associated mouse event for dragging
	 * @param LocalMousePos		The current location of the mouse, relative to the top-left corner of the physical track area
	 * @param VirtualTrackArea	A virtual track area that can be used for pixel->time conversions and hittesting
	 */
	virtual void OnEndDrag( const FPointerEvent& MouseEvent, FVector2D LocalMousePos, const FVirtualTrackArea& VirtualTrackArea) = 0;

	/** Request the cursor for this drag operation */
	virtual FCursorReply GetCursor() const = 0;

	/** Override to implement drag-specific paint logic */
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const = 0;

public:

	/** Virtual destructor. */
	virtual ~ISequencerEditToolDragOperation() { }
};


/**
 * Interface for edit tools in Sequencer.
 */
class ISequencerEditTool
{
public:

	// @todo sequencer: documentation needed
	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual FReply OnMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual void OnMouseCaptureLost() = 0;
	virtual void OnMouseEnter(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;
	virtual void OnMouseLeave(SWidget& OwnerWidget, const FPointerEvent& MouseEvent) = 0;
	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const = 0;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) = 0;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const = 0;
	virtual ISequencer& GetSequencer() const = 0;
	virtual FName GetIdentifier() const = 0;

	/** Get the current active hotspot */
	virtual TSharedPtr<ISequencerHotspot> GetHotspot() const = 0;

	/** Set the hotspot to something else */
	virtual void SetHotspot(TSharedPtr<ISequencerHotspot> NewHotspot) = 0;

public:

	/** Virtual destructor. */
	virtual ~ISequencerEditTool() { }
};
