// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SCurveEditor.h"
#include "SequencerSelection.h"
#include "Sequencer.h"

#define LOCTEXT_NAMESPACE "SequencerCurveEditor"

void SSequencerCurveEditor::Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer )
{
	ViewRange = InArgs._ViewRange;
	Sequencer = InSequencer;
	NodeTreeSelectionChangedHandle = Sequencer.Pin()->GetSelection()->GetOnOutlinerNodeSelectionChanged()->AddSP(this, &SSequencerCurveEditor::NodeTreeSelectionChanged);
	GetMutableDefault<USequencerSettings>()->GetOnCurveVisibilityChanged()->AddSP( this, &SSequencerCurveEditor::SequencerCurveVisibilityChanged );

	ChildSlot
	[
		SAssignNew( CurveEditor, SCurveEditor )
		.ViewMinInput_Lambda( [this]{ return ViewRange.Get().GetLowerBoundValue(); } )
		.ViewMaxInput_Lambda( [this]{ return ViewRange.Get().GetUpperBoundValue(); } )
		.HideUI( false )
		.ShowCurveSelector( false )
		.ShowZoomButtons( false )
		.ShowInputGridNumbers( false )
		.SnappingEnabled( this, &SSequencerCurveEditor::GetCurveSnapEnabled )
		.InputSnap( this, &SSequencerCurveEditor::GetCurveTimeSnapInterval )
		.OutputSnap( this, &SSequencerCurveEditor::GetCurveValueSnapInterval )
		.TimelineLength( 0.0f )
		.ShowCurveToolTips( this, &SSequencerCurveEditor::GetShowCurveEditorCurveToolTips )
		.GridColor( FLinearColor( 0.3f, 0.3f, 0.3f, 0.3f ) )
	];
}

template<class ParentNodeType>
TSharedPtr<ParentNodeType> GetParentOfType( TSharedRef<FSequencerDisplayNode> InNode, ESequencerNode::Type InNodeType )
{
	TSharedPtr<FSequencerDisplayNode> CurrentNode = InNode->GetParent();
	while ( CurrentNode.IsValid() )
	{
		if ( CurrentNode->GetType() == InNodeType )
		{
			break;
		}
		CurrentNode = CurrentNode->GetParent();
	}
	return StaticCastSharedPtr<ParentNodeType>( CurrentNode );
}

void SSequencerCurveEditor::SetSequencerNodeTree( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree )
{
	SequencerNodeTree = InSequencerNodeTree;
	UpdateCurveOwner();
}

void SSequencerCurveEditor::UpdateCurveOwner()
{
	CurveOwner = MakeShareable( new FSequencerCurveOwner( SequencerNodeTree ) );
	CurveEditor->SetCurveOwner( CurveOwner.Get() );
}

bool SSequencerCurveEditor::GetCurveSnapEnabled() const
{
	const USequencerSettings* Settings = GetDefault<USequencerSettings>();
	return Settings->GetIsSnapEnabled();
}

float SSequencerCurveEditor::GetCurveTimeSnapInterval() const
{
	return GetDefault<USequencerSettings>()->GetSnapKeyTimesToInterval()
		? GetDefault<USequencerSettings>()->GetTimeSnapInterval()
		: 0;
}

float SSequencerCurveEditor::GetCurveValueSnapInterval() const
{
	return GetDefault<USequencerSettings>()->GetSnapCurveValueToInterval()
		? GetDefault<USequencerSettings>()->GetCurveValueSnapInterval()
		: 0;
}

bool SSequencerCurveEditor::GetShowCurveEditorCurveToolTips() const
{
	return GetDefault<USequencerSettings>()->GetShowCurveEditorCurveToolTips();
}

void SSequencerCurveEditor::NodeTreeSelectionChanged()
{
	if (SequencerNodeTree.IsValid())
	{
		if (GetDefault<USequencerSettings>()->GetCurveVisibility() == ESequencerCurveVisibility::SelectedCurves)
		{
			UpdateCurveOwner();
		}
	}
}

void SSequencerCurveEditor::SequencerCurveVisibilityChanged()
{
	UpdateCurveOwner();
}

TSharedPtr<FUICommandList> SSequencerCurveEditor::GetCommands()
{
	return CurveEditor->GetCommands();
}

SSequencerCurveEditor::~SSequencerCurveEditor()
{
	if ( Sequencer.IsValid() )
	{
		Sequencer.Pin()->GetSelection()->GetOnOutlinerNodeSelectionChanged()->Remove( NodeTreeSelectionChangedHandle );
	}
}

#undef LOCTEXT_NAMESPACE