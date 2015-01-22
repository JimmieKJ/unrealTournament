// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"

#include "SAnimationOutlinerView.h"
#include "ScopedTransaction.h"
#include "Sequencer.h"
#include "MovieScene.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "AnimationOutliner"


void SAnimationOutlinerTreeNode::Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node )
{
	DisplayNode = Node;
	OnSelectionChanged = InArgs._OnSelectionChanged;

	SelectedBrush = FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.SelectionBorder" );
	NotSelectedBrush = FEditorStyle::GetBrush( "NoBorder" );
	ExpandedBrush = FEditorStyle::GetBrush( "TreeArrow_Expanded" );
	CollapsedBrush = FEditorStyle::GetBrush( "TreeArrow_Collapsed" );

	// Choose the font.  If the node is a root node, we show a larger font for it.
	FSlateFontInfo NodeFont = Node->GetParent().IsValid() ? 
		FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont") :
		FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.BoldFont");

	TSharedRef<SWidget> TextWidget = 
		SNew( STextBlock )
		.Text( Node->GetDisplayName() )
		.Font( NodeFont );

	TSharedRef<SWidget>	FinalWidget = 
		SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign( VAlign_Center )
		[
			SNew(SButton)
			.ContentPadding(0)
			.Visibility( this, &SAnimationOutlinerTreeNode::GetExpanderVisibility )
			.OnClicked( this, &SAnimationOutlinerTreeNode::OnExpanderClicked )
			.ClickMethod( EButtonClickMethod::MouseDown )
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			[
				SNew( SImage )
				.Image( this, &SAnimationOutlinerTreeNode::OnGetExpanderImage)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign( VAlign_Center )
		[
			TextWidget
		];


	ChildSlot
	[
		SNew( SBorder )
		.VAlign( VAlign_Center )
		.Padding(1.0f)
		.BorderImage( this, &SAnimationOutlinerTreeNode::GetNodeBorderImage )
		[
			FinalWidget
		]
	];

	SetVisibility( TAttribute<EVisibility>( this, &SAnimationOutlinerTreeNode::GetNodeVisibility ) );
}


FReply SAnimationOutlinerTreeNode::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && DisplayNode->IsSelectable() )
	{
		bool bSelected = DisplayNode->IsSelected();

		if( MouseEvent.IsControlDown() )
		{
			const bool bDeselectOtherNodes = false;
			// Select the node if we were clicked on
			DisplayNode->SetSelectionState( !bSelected, bDeselectOtherNodes );
		}
		else
		{
			const bool bDeselectOtherNodes = true;
			// Select the node if we were clicked on
			DisplayNode->SetSelectionState( true, bDeselectOtherNodes );
		}

		OnSelectionChanged.ExecuteIfBound( DisplayNode );
		return FReply::Handled();
	}
	else if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SAnimationOutlinerTreeNode::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && HasMouseCapture() )
	{
		TSharedPtr<SWidget> MenuContent = DisplayNode->OnSummonContextMenu(MyGeometry, MouseEvent);
		if (MenuContent.IsValid())
		{
			FSlateApplication::Get().PushMenu(
				AsShared(),
				MenuContent.ToSharedRef(),
				MouseEvent.GetScreenSpacePosition(),
				FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
				);
			
			return FReply::Handled().ReleaseMouseCapture().SetUserFocus(MenuContent.ToSharedRef(), EFocusCause::SetDirectly);
		}

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SAnimationOutlinerTreeNode::OnExpanderClicked() 
{
	DisplayNode->ToggleExpansion();
	return FReply::Handled();
}

const FSlateBrush* SAnimationOutlinerTreeNode::GetNodeBorderImage() const
{		
	// Display a highlight when the node is selected
	const bool bIsSelected = DisplayNode->IsSelected();
	return bIsSelected ? SelectedBrush : NotSelectedBrush;
}

const FSlateBrush* SAnimationOutlinerTreeNode::OnGetExpanderImage() const
{
	const bool bIsExpanded = DisplayNode->IsExpanded();
	return bIsExpanded ? ExpandedBrush : CollapsedBrush;
}

EVisibility SAnimationOutlinerTreeNode::GetNodeVisibility() const
{
	return DisplayNode->IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAnimationOutlinerTreeNode::GetExpanderVisibility() const
{
	return DisplayNode->GetNumChildren() > 0 ? EVisibility::Visible : EVisibility::Hidden;
}

void SAnimationOutlinerView::Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> InRootNode, TSharedRef<FSequencer> InSequencer )
{
	SAnimationOutlinerViewBase::Construct( SAnimationOutlinerViewBase::FArguments(), InRootNode );

	Sequencer = InSequencer;

	GenerateWidgetForNode( InRootNode );

	SetVisibility( TAttribute<EVisibility>( this, &SAnimationOutlinerView::GetNodeVisibility ) );
}

SAnimationOutlinerView::~SAnimationOutlinerView()
{
}

EVisibility SAnimationOutlinerView::GetNodeVisibility() const
{
	return RootNode->IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}


void SAnimationOutlinerView::GenerateWidgetForNode( TSharedRef<FSequencerDisplayNode>& InLayoutNode )
{
	Children.Add( 
		SNew( SAnimationOutlinerTreeNode, InLayoutNode)
		.OnSelectionChanged( this, &SAnimationOutlinerView::OnSelectionChanged )
		);

	// Object nodes do not generate widgets for their children
	if( InLayoutNode->GetType() != ESequencerNode::Object )
	{
		const TArray< TSharedRef<FSequencerDisplayNode> >& ChildNodes = InLayoutNode->GetChildNodes();
		
		for( int32 ChildIndex = 0; ChildIndex < ChildNodes.Num(); ++ChildIndex )
		{
			TSharedRef<FSequencerDisplayNode> ChildNode = ChildNodes[ChildIndex];
			GenerateWidgetForNode( ChildNode );
		}
	}
}


void SAnimationOutlinerView::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const float Padding = SequencerLayoutConstants::NodePadding;
	const float IndentAmount = SequencerLayoutConstants::IndentAmount;

	const float Scale = 1.0f;
	float CurrentHeight = 0;
	for (int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex)
	{
		const TSharedRef<SAnimationOutlinerTreeNode>& Widget = Children[WidgetIndex];

		EVisibility Visibility = Widget->GetVisibility();
		if( ArrangedChildren.Accepts( Visibility ) )
		{
			const TSharedPtr<const FSequencerDisplayNode>& DisplayNode = Widget->GetDisplayNode();
			// How large to make this node
			float HeightIncrement = DisplayNode->GetNodeHeight();
			// How far to indent the widget
			float WidgetIndentOffset = IndentAmount*DisplayNode->GetTreeLevel();
			// Place the widget at the current height, at the nodes desired size
			ArrangedChildren.AddWidget( 
				Visibility, 
				AllottedGeometry.MakeChild( Widget, FVector2D( WidgetIndentOffset, CurrentHeight ), FVector2D( AllottedGeometry.GetDrawSize().X-WidgetIndentOffset, HeightIncrement ), Scale ) 
				);
		
			// Compute the start height for the next widget
			CurrentHeight += HeightIncrement+Padding;
		}
	}
}


void SAnimationOutlinerView::OnSelectionChanged( TSharedPtr<FSequencerDisplayNode> AffectedNode )
{
	// Select objects bound to the object node
	if( AffectedNode->GetType() == ESequencerNode::Object )
	{
		const TSharedPtr<const FObjectBindingNode> ObjectNode = StaticCastSharedPtr<const FObjectBindingNode>( AffectedNode );

		// Get the bound objects
		TArray<UObject*> RuntimeObjects;
		Sequencer.Pin()->GetRuntimeObjects( Sequencer.Pin()->GetFocusedMovieSceneInstance(), ObjectNode->GetObjectBinding(), RuntimeObjects );
		
		if( RuntimeObjects.Num() > 0 && Sequencer.Pin()->IsLevelEditorSequencer() )
		{
			const bool bNotifySelectionChanged = false;
			const bool bDeselectBSP = true;
			const bool bWarnAboutTooManyActors = false;

			// Clear selection
			GEditor->SelectNone(bNotifySelectionChanged,bDeselectBSP,bWarnAboutTooManyActors);
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();

			// Select each actor
			bool bActorSelected = false;
			for( int32 ObjectIndex = 0; ObjectIndex < RuntimeObjects.Num(); ++ObjectIndex )
			{
				AActor* Actor = Cast<AActor>( RuntimeObjects[ObjectIndex] );

				if( Actor )
				{
					const bool bSelectActor = true;

					GEditor->SelectActor(Actor, bSelectActor, bNotifySelectionChanged );
					bActorSelected = true;
				}
			}

			GEditor->GetSelectedActors()->EndBatchSelectOperation();

			if( bActorSelected )
			{
				GEditor->NoteSelectionChange();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE