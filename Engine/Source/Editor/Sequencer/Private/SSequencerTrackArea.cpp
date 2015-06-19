// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SSequencerTrackArea.h"
#include "ISequencerSection.h"
#include "MovieSceneSection.h"
#include "SSequencer.h"
#include "TimeSliderController.h"
#include "SSequencerSectionAreaView.h"

void SSequencerTrackArea::Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer )
{
	ViewRange = InArgs._ViewRange;
	OutlinerFillPercent = InArgs._OutlinerFillPercent;
	Sequencer = InSequencer;

	TSharedRef<SScrollBar> ScrollBar =
		SNew(SScrollBar)
		.Thickness(FVector2D(5.0f, 5.0f));

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(ScrollBox, SScrollBox)
			.ExternalScrollbar(ScrollBar)
		]
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(OutlinerFillPercent)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(CurveEditor, SSequencerCurveEditor, InSequencer)
				.Visibility( this, &SSequencerTrackArea::GetCurveEditorVisibility )
				.ViewRange(ViewRange)
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.FillWidth( TAttribute<float>( this, &SSequencerTrackArea::GetScrollBarSlotFill ) )
			[
				ScrollBar
			]
			+ SHorizontalBox::Slot()
			.FillWidth( TAttribute<float>( this, &SSequencerTrackArea::GetScrollBarSpacerSlotFill ) )
			[
				SNew( SSpacer )
			]
		]
	];
}

void SSequencerTrackArea::Update( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree )
{
	ScrollBox->ClearChildren();

	const TArray< TSharedRef<FSequencerDisplayNode> >& RootNodes = InSequencerNodeTree->GetRootNodes();

	for( int32 RootNodeIndex = 0; RootNodeIndex < RootNodes.Num(); ++RootNodeIndex )
	{
		TSharedRef<FSequencerDisplayNode> RootNode = RootNodes[RootNodeIndex];

		// Generate a widget for each root node
		GenerateWidgetForNode( RootNode );

		// Generate a node for each children of the root node
		GenerateLayoutNodeWidgetsRecursive( RootNode->GetChildNodes() );
	}

	// @todo Sequencer - Remove this expensive operation
	ScrollBox->SlatePrepass();

	CurveEditor->SetSequencerNodeTree(InSequencerNodeTree);
}

void SSequencerTrackArea::GenerateLayoutNodeWidgetsRecursive( const TArray< TSharedRef<FSequencerDisplayNode> >& Nodes )
{
	for( int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex )
	{
		TSharedRef<FSequencerDisplayNode> Node = Nodes[NodeIndex];

		// The section area encompasses multiple logical child nodes so we do not generate children that arent section areas (they would not be top level nodes in that case)
		if( Node->GetType() == ESequencerNode::Track )
		{
			GenerateWidgetForNode( Node );	

			GenerateLayoutNodeWidgetsRecursive( Node->GetChildNodes() );
		}
	}
}

void SSequencerTrackArea::GenerateWidgetForNode( TSharedRef<FSequencerDisplayNode>& Node )
{
	ScrollBox->AddSlot()
	[
		SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.FillWidth( OutlinerFillPercent )
		.Padding( FMargin(0.0f, 2.0f ) )
		[
			// Generate a widget for the animation outliner portion of the node
			Node->GenerateWidgetForOutliner( Sequencer.Pin().ToSharedRef() )
		]
		+ SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		.Padding( FMargin(0.0f, 2.0f) )
		[
			SNew(SBox)
			.Visibility(this, &SSequencerTrackArea::GetSectionControlVisibility)
			[
				// Generate a widget for the section area portion of the node
				Node->GenerateWidgetForSectionArea( ViewRange )
			]
		]
	];
}

EVisibility SSequencerTrackArea::GetCurveEditorVisibility() const
{
	return GetDefault<USequencerSettings>()->GetShowCurveEditor() ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SSequencerTrackArea::GetSectionControlVisibility() const
{
	return GetDefault<USequencerSettings>()->GetShowCurveEditor() ? EVisibility::Hidden : EVisibility::Visible;
}

float SSequencerTrackArea::GetScrollBarSlotFill() const
{
	return GetDefault<USequencerSettings>()->GetShowCurveEditor() ? OutlinerFillPercent.Get() : 1.0f;
}

float SSequencerTrackArea::GetScrollBarSpacerSlotFill() const
{
	return GetDefault<USequencerSettings>()->GetShowCurveEditor() ? 1.0f : 0.0f;
}
