// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DesignerExtension.h"
#include "Layout/Anchors.h"

class UCanvasPanel;
class UCanvasPanelSlot;

/** Set of anchor widget types */
namespace EAnchorWidget
{
	enum Type
	{
		Center,
		Left,
		Right,
		Top,
		Bottom,
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight,

		MAX_COUNT
	};
}

/**
 * The canvas slot extension provides design time widgets for widgets that are selected in the canvas.
 */
class FCanvasSlotExtension : public FDesignerExtension
{
public:
	FCanvasSlotExtension();

	virtual ~FCanvasSlotExtension() {}

	virtual bool CanExtendSelection(const TArray< FWidgetReference >& Selection) const override;
	
	virtual void ExtendSelection(const TArray< FWidgetReference >& Selection, TArray< TSharedRef<FDesignerSurfaceElement> >& SurfaceElements) override;
	virtual void Paint(const TSet< FWidgetReference >& Selection, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;

private:

	FReply HandleAnchorBeginDrag(const FGeometry& Geometry, const FPointerEvent& Event, EAnchorWidget::Type AnchorType);
	FReply HandleAnchorEndDrag(const FGeometry& Geometry, const FPointerEvent& Event, EAnchorWidget::Type AnchorType);
	FReply HandleAnchorDragging(const FGeometry& Geometry, const FPointerEvent& Event, EAnchorWidget::Type AnchorType);

	TSharedRef<SWidget> MakeAnchorWidget(EAnchorWidget::Type AnchorType, float Width, float Height);

	void OnMouseEnterAnchor();
	void OnMouseLeaveAnchor();

	const FSlateBrush* GetAnchorBrush(EAnchorWidget::Type AnchorType) const;
	EVisibility GetAnchorVisibility(EAnchorWidget::Type AnchorType) const;
	FVector2D GetAnchorAlignment(EAnchorWidget::Type AnchorType) const;

	static bool GetCollisionSegmentsForSlot(UCanvasPanel* Canvas, int32 SlotIndex, TArray<FVector2D>& Segments);
	static bool GetCollisionSegmentsForSlot(UCanvasPanel* Canvas, UCanvasPanelSlot* Slot, TArray<FVector2D>& Segments);
	static void GetCollisionSegmentsFromGeometry(FGeometry ArrangedGeometry, TArray<FVector2D>& Segments);

	void PaintCollisionLines(const TSet< FWidgetReference >& Selection, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintDragPercentages(const TSet< FWidgetReference >& Selection, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintLineWithText(FVector2D Start, FVector2D End, FText Text, FVector2D TextTransform, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	void ProximitySnapValue(float SnapFrequency, float SnapProximity, float& Value);

private:

	/** */
	TArray< TSharedPtr<SWidget> > AnchorWidgets;

	/** */
	bool bMovingAnchor;

	/** */
	bool bHoveringAnchor;

	/** */
	FVector2D MouseDownPosition;

	/** */
	FAnchors BeginAnchors;
};