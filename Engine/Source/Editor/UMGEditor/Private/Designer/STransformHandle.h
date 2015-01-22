// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"

namespace ETransformDirection
{
	enum Type
	{
		TopLeft = 0,
		TopCenter,
		TopRight,

		CenterLeft,
		CenterRight,

		BottomLeft,
		BottomCenter,
		BottomRight,

		MAX
	};
}


namespace ETransformAction
{
	enum Type
	{
		None,
		Primary,
		Secondary
	};
}

class STransformHandle : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STransformHandle) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, IUMGDesigner* InDesigner, ETransformDirection::Type InTransformDirection);

	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	// End SWidget interface
	
protected:
	EVisibility GetHandleVisibility() const;

	ETransformAction::Type ComputeActionAtLocation(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const;

protected:
	FVector2D ComputeDragDirection(ETransformDirection::Type InTransformDirection) const;
	FVector2D ComputeOrigin(ETransformDirection::Type InTransformDirection) const;

protected:
	IUMGDesigner* Designer;
	ETransformDirection::Type TransformDirection;
	ETransformAction::Type Action;

	FVector2D DragDirection;
	FVector2D DragOrigin;
};
