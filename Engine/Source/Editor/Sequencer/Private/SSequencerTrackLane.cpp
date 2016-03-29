// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SSequencerTrackLane.h"
#include "SSequencerTreeView.h"


void SSequencerTrackLane::Construct(const FArguments& InArgs, const TSharedRef<FSequencerDisplayNode>& InDisplayNode, const TSharedRef<SSequencerTreeView>& InTreeView)
{
	DisplayNode = InDisplayNode;
	TreeView = InTreeView;

	ChildSlot
	.HAlign(HAlign_Fill)
	.Padding(0)
	[
		InArgs._Content.Widget
	];
}


void DrawLaneRecursive(const TSharedRef<FSequencerDisplayNode>& DisplayNode, const FGeometry& AllottedGeometry, float& YOffs, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle)
{
	static const FName BorderName("Sequencer.AnimationOutliner.DefaultBorder");
	static const FName SelectionColorName("SelectionColor");

	if (DisplayNode->IsHidden())
	{
		return;
	}
	
	float TotalNodeHeight = DisplayNode->GetNodeHeight() + DisplayNode->GetNodePadding().Combined();

	// draw selection border
	FSequencerSelection& SequencerSelection = DisplayNode->GetSequencer().GetSelection();

	if (SequencerSelection.IsSelected(DisplayNode))
	{
		FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor(InWidgetStyle);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(0, YOffs),
				FVector2D(AllottedGeometry.Size.X, TotalNodeHeight)
			),
			FEditorStyle::GetBrush(BorderName),
			MyClippingRect,
			ESlateDrawEffect::None,
			SelectionColor
		);
	}

	// draw hovered or node has keys or sections selected border
	else
	{
		FLinearColor HighlightColor;
		bool bDrawHighlight = false;
		if (SequencerSelection.NodeHasSelectedKeysOrSections(DisplayNode))
		{
			bDrawHighlight = true;
			HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);
		}
		else if (DisplayNode->IsHovered())
		{
			bDrawHighlight = true;
			HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.05f);
		}

		if (bDrawHighlight)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(
					FVector2D(0, YOffs),
					FVector2D(AllottedGeometry.Size.X, TotalNodeHeight)
				),
				FEditorStyle::GetBrush(BorderName),
				MyClippingRect,
				ESlateDrawEffect::None,
				HighlightColor
			);
		}
	}

	YOffs += TotalNodeHeight;

/*	// draw lane separator
	static const FName LaneColorName("Sequencer.TrackArea.LaneColor");

	TArray<FVector2D> Points;
	{
		Points.Emplace(0, YOffs);
		Points.Emplace(PaintGeometry.GetLocalSize().X, YOffs);
	}

	FSlateDrawElement::MakeLines( 
		OutDrawElements, 
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		MyClippingRect,
		ESlateDrawEffect::None,
		FEditorStyle::GetSlateColor(LaneColorName).GetColor(InWidgetStyle),
		false //bAntialias
	);*/

	if (DisplayNode->IsExpanded())
	{
		for (const auto& ChildNode : DisplayNode->GetChildNodes())
		{
			DrawLaneRecursive(ChildNode, AllottedGeometry, YOffs, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle);
		}
	}
}


int32 SSequencerTrackLane::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	float YOffs = 0;
	DrawLaneRecursive(DisplayNode.ToSharedRef(), AllottedGeometry, YOffs, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle);

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
}

void SSequencerTrackLane::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	FVector2D ThisFrameDesiredSize = GetDesiredSize();

	if (LastDesiredSize.IsSet() && ThisFrameDesiredSize.Y != LastDesiredSize.GetValue().Y)
	{
		TSharedPtr<SSequencerTreeView> PinnedTree = TreeView.Pin();
		if (PinnedTree.IsValid())
		{
			PinnedTree->RequestTreeRefresh();
		}
	}

	LastDesiredSize = ThisFrameDesiredSize;
}

FVector2D SSequencerTrackLane::ComputeDesiredSize(float LayoutScale) const
{
	float Height = DisplayNode->GetNodeHeight() + DisplayNode->GetNodePadding().Combined();
	
	
	if (DisplayNode->GetType() == ESequencerNode::Track || DisplayNode->GetType() == ESequencerNode::Category)
	{
		const bool bIncludeThisNode = false;
		
		// These types of nodes need to consider the entire visible hierarchy for their desired size
		DisplayNode->TraverseVisible_ParentFirst([&](FSequencerDisplayNode& InNode){
			Height += InNode.GetNodeHeight() + InNode.GetNodePadding().Combined();
			return true;
		}, bIncludeThisNode);
	}

	return FVector2D(100.f, Height);
}

float SSequencerTrackLane::GetPhysicalPosition() const
{
	return TreeView.Pin()->ComputeNodePosition(DisplayNode.ToSharedRef()).Get(0.f);
}
