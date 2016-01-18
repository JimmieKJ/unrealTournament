// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SSequencerTrackLane.h"
#include "SSequencerTreeView.h"

void SSequencerTrackLane::Construct(const FArguments& InArgs, const TSharedRef<FSequencerDisplayNode>& InDisplayNode, const TSharedRef<SSequencerTreeView>& InTreeView)
{
	DisplayNode = InDisplayNode;
	TreeView = InTreeView;

	ChildSlot
	.HAlign(HAlign_Fill)
	.Padding(0, DisplayNode->GetNodePadding().Top, 0, 0)
	[
		InArgs._Content.Widget
	];
}

int32 SSequencerTrackLane::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Draw a separator below the track
	auto PaintGeometry = AllottedGeometry.ToPaintGeometry();

	TArray<FVector2D> Points;
	Points.Emplace(0, PaintGeometry.GetLocalSize().Y);
	Points.Emplace(PaintGeometry.GetLocalSize().X, PaintGeometry.GetLocalSize().Y);

	static FLinearColor SeparatorColor(0.f, 0.f, 0.f, 0.15f);

	FSlateDrawElement::MakeLines( 
		OutDrawElements, 
		LayerId,
		PaintGeometry,
		Points,
		MyClippingRect,
		ESlateDrawEffect::None,
		SeparatorColor
		);

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}

float SSequencerTrackLane::GetPhysicalPosition() const
{
	// Positioning strategy:
	// We know that there must be at least one tree view row around that references something in our subtree (otherwise this widget wouldn't exist)
	// Knowing this, we just need to root it out by traversing the visible tree, and asking for the first rows's offset
	float NegativeOffset = 0.f;
	TOptional<float> Top;

	auto TreeViewPinned = TreeView.Pin();
	
	// Iterate parent first until we find a tree view row we can use for the offset height
	auto Iter = [&](FSequencerDisplayNode& InNode){
		
		auto ChildRowGeometry = TreeViewPinned->GetPhysicalGeometryForNode(InNode.AsShared());
		if (ChildRowGeometry.IsSet())
		{
			Top = ChildRowGeometry->PhysicalTop;
			// Stop iterating
			return false;
		}

		NegativeOffset -= InNode.GetNodeHeight() + InNode.GetNodePadding().Combined();
		return true;
	};

	DisplayNode->TraverseVisible_ParentFirst(Iter);

	if (!Top.IsSet())
	{
		Top = 0.f;
	}

	return NegativeOffset + Top.GetValue();
}