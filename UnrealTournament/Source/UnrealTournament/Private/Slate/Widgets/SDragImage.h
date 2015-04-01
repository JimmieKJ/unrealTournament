// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"

#if !UE_SERVER

DECLARE_DELEGATE_OneParam(FDragHandler, FVector2D);
DECLARE_DELEGATE_OneParam(FZoomHandler, float);

class SDragImage : public SImage
{
public:
	SLATE_BEGIN_ARGS(SDragImage)
		: _Image(FCoreStyle::Get().GetDefaultBrush())
		, _ColorAndOpacity(FLinearColor::White)
		, _OnDrag()
		, _OnZoom()
	{}

		/** Image resource */
		SLATE_ATTRIBUTE(const FSlateBrush*, Image)

		/** Color and opacity */
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)

		/** Invoked when the mouse is dragged in the widget */
		SLATE_EVENT(FDragHandler, OnDrag)

		/** Invoked when the mouse is scrolled in the widget */
		SLATE_EVENT(FZoomHandler, OnZoom)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnDrag = InArgs._OnDrag;
		OnZoom = InArgs._OnZoom;
		SImage::Construct(SImage::FArguments().Image(InArgs._Image).ColorAndOpacity(InArgs._ColorAndOpacity));
	}

	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse).CaptureMouse(AsShared());
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (HasMouseCapture())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		else
		{
			return FReply::Handled();
		}
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (HasMouseCapture())
		{
			OnDrag.ExecuteIfBound(MouseEvent.GetCursorDelta());
		}
		return FReply::Handled();
	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		OnZoom.ExecuteIfBound(MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}

protected:
	FDragHandler OnDrag;
	FZoomHandler OnZoom;
};

#endif