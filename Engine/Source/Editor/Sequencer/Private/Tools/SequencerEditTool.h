// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerEditTool.h"


/**
 * Abstract base class for edit tools.
 */
class FSequencerEditTool
	: public ISequencerEditTool
{
public:

	/** Virtual destructor. */
	~FSequencerEditTool() { }

public:

	// ISequencerEditTool interface

	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	virtual FReply OnMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	virtual void OnMouseCaptureLost() override
	{
		// do nothing
	}

	virtual void OnMouseEnter(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		// do nothing
	}

	virtual void OnMouseLeave(SWidget& OwnerWidget, const FPointerEvent& MouseEvent) override
	{
		// do nothing
	}

	virtual int32 OnPaint(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override
	{
		return LayerId;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		// do nothing
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		return FCursorReply::Unhandled();
	}

	/** Get the current active hotspot */
	virtual TSharedPtr<ISequencerHotspot> GetHotspot() const override
	{
		return Hotspot;
	}

	/** Set the hotspot to something else */
	virtual void SetHotspot(TSharedPtr<ISequencerHotspot> NewHotspot) override
	{
		Hotspot = MoveTemp(NewHotspot);
	}

protected:

	/** The current hotspot that can be set from anywhere to initiate drags */
	TSharedPtr<ISequencerHotspot> Hotspot;
};
