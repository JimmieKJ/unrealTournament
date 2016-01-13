// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"

#if !UE_SERVER

DECLARE_DELEGATE_TwoParams(FMouseHandler, const FGeometry&, const FPointerEvent&);
DECLARE_DELEGATE_OneParam(FZoomHandler, float);

class UNREALTOURNAMENT_API SDragImage : public SImage
{
public:
	SLATE_BEGIN_ARGS(SDragImage)
		: _Image(FCoreStyle::Get().GetDefaultBrush())
		, _ColorAndOpacity(FLinearColor::White)
		, _OnDrag()
		, _OnMove()
		, _OnMousePressed()
		, _OnMouseReleased()
		, _OnZoom()
	{}

		/** Image resource */
		SLATE_ATTRIBUTE(const FSlateBrush*, Image)

		/** Color and opacity */
		SLATE_ATTRIBUTE(FSlateColor, ColorAndOpacity)

		/** Invoked when the mouse is dragged in the widget */
		SLATE_EVENT(FMouseHandler, OnDrag)

		/** Invoked when the mouse is dragged in the widget */
		SLATE_EVENT(FMouseHandler, OnMove)

		/** Invoked when a mouse button is pressed in the widget */
		SLATE_EVENT(FMouseHandler, OnMousePressed)

		/** Invoked when a mouse button is released in the widget */
		SLATE_EVENT(FMouseHandler, OnMouseReleased)

		/** Invoked when the mouse is scrolled in the widget */
		SLATE_EVENT(FZoomHandler, OnZoom)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnDrag = InArgs._OnDrag;
		OnZoom = InArgs._OnZoom;
		OnMove = InArgs._OnMove;
		OnMousePressed = InArgs._OnMousePressed;
		OnMouseReleased = InArgs._OnMouseReleased;
		SImage::Construct(SImage::FArguments().Image(InArgs._Image).ColorAndOpacity(InArgs._ColorAndOpacity));
	}

	virtual bool SupportsKeyboardFocus() const
	{
		return true;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		OnMousePressed.ExecuteIfBound(MyGeometry, MouseEvent);
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse).CaptureMouse(AsShared());
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		OnMouseReleased.ExecuteIfBound(MyGeometry, MouseEvent);
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
			OnDrag.ExecuteIfBound(MyGeometry, MouseEvent);
		}
		OnMove.ExecuteIfBound(MyGeometry, MouseEvent);
		return FReply::Handled();
	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		OnZoom.ExecuteIfBound(MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}

protected:
	FMouseHandler OnDrag;
	FMouseHandler OnMove;
	FMouseHandler OnMousePressed;
	FMouseHandler OnMouseReleased;
	FZoomHandler OnZoom;
};

#endif