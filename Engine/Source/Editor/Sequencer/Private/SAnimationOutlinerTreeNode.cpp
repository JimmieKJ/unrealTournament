// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SAnimationOutlinerTreeNode.h"
#include "ScopedTransaction.h"
#include "Sequencer.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "MovieSceneCommonHelpers.h"
#include "Engine/Selection.h"
#include "IKeyArea.h"
#include "SEditableLabel.h"
#include "SSequencerTreeView.h"


#define LOCTEXT_NAMESPACE "AnimationOutliner"


void SAnimationOutlinerTreeNode::Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node, const TSharedRef<SSequencerTreeViewRow>& InTableRow )
{
	DisplayNode = Node;

	SelectedBrush = FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.SelectionBorder" );
	SelectedBrushInactive = FEditorStyle::GetBrush("Sequencer.AnimationOutliner.SelectionBorderInactive");
	NotSelectedBrush = FEditorStyle::GetBrush( "NoBorder" );
	TableRowStyle = &FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");

	// Choose the font.  If the node is a root node or an object node, we show a larger font for it.
	FSlateFontInfo NodeFont = Node->GetParent().IsValid() && Node->GetType() != ESequencerNode::Object ? 
		FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont") :
		FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.BoldFont");

	EditableLabel = SNew(SEditableLabel)
		.CanEdit(this, &SAnimationOutlinerTreeNode::HandleNodeLabelCanEdit)
		.Font(NodeFont)
		.OnTextChanged(this, &SAnimationOutlinerTreeNode::HandleNodeLabelTextChanged)
		.Text(this, &SAnimationOutlinerTreeNode::GetDisplayName);

	auto NodeHeight = [=]() -> FOptionalSize { return DisplayNode->GetNodeHeight(); };

	TAttribute<FLinearColor> HoverTint(this, &SAnimationOutlinerTreeNode::GetHoverTint);
	ForegroundColor.Bind(this, &SAnimationOutlinerTreeNode::GetForegroundBasedOnSelection);

	TSharedRef<SWidget>	FinalWidget = 
		SNew( SBorder )
		.VAlign( VAlign_Center )
		.BorderImage( this, &SAnimationOutlinerTreeNode::GetNodeBorderImage )
		.Padding(FMargin(0, Node->GetNodePadding().Combined() / 2))
		[
			SNew(SBox)
			.HeightOverride_Lambda(NodeHeight)
			[
				SNew( SHorizontalBox )

				// Expand track lanes button
				+ SHorizontalBox::Slot()
				.Padding(FMargin(2.f, 0.f))
				.VAlign( VAlign_Center )
				.AutoWidth()
				[
					SNew(SExpanderArrow, InTableRow).IndentAmount(SequencerLayoutConstants::IndentAmount)
				]

				// Label Slot
				+ SHorizontalBox::Slot()
				.VAlign( VAlign_Center )
				.FillWidth(3)
				[
					EditableLabel.ToSharedRef()
				]

				// Editor slot
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					// @todo Sequencer - Remove this box and width override.
					SNew(SBox)
					.HAlign(HAlign_Left)
					.WidthOverride(100)
					[
						DisplayNode->GenerateEditWidgetForOutliner()
					]
				]
				// Previous key slot
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(3, 0, 0, 0)
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage(NotSelectedBrush)
					.ColorAndOpacity(HoverTint)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton")
						.ToolTipText(LOCTEXT("PreviousKeyButton", "Set the time to the previous key"))
						.OnClicked(this, &SAnimationOutlinerTreeNode::OnPreviousKeyClicked)
						.ForegroundColor( FSlateColor::UseForeground() )
						.ContentPadding(0)
						[
							SNew(STextBlock)
							.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
							.Text(FText::FromString(FString(TEXT("\xf060"))) /*fa-arrow-left*/)
						]
					]
				]
				// Add key slot
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage(NotSelectedBrush)
					.ColorAndOpacity(HoverTint)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton")
						.ToolTipText(LOCTEXT("AddKeyButton", "Add a new key at the current time"))
						.OnClicked(this, &SAnimationOutlinerTreeNode::OnAddKeyClicked)
						.ForegroundColor( FSlateColor::UseForeground() )
						.ContentPadding(0)
						[
							SNew(STextBlock)
							.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
							.Text(FText::FromString(FString(TEXT("\xf055"))) /*fa-plus-circle*/)
						]
					]
				]
				// Next key slot
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage(NotSelectedBrush)
					.ColorAndOpacity(HoverTint)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "FlatButton")
						.ToolTipText(LOCTEXT("NextKeyButton", "Set the time to the next key"))
						.OnClicked(this, &SAnimationOutlinerTreeNode::OnNextKeyClicked)
						.ContentPadding(0)
						.ForegroundColor( FSlateColor::UseForeground() )
						[
							SNew(STextBlock)
							.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
							.Text(FText::FromString(FString(TEXT("\xf061"))) /*fa-arrow-right*/)
						]
					]
				]
			]
		];

	ChildSlot
	[
		FinalWidget
	];
}


void SAnimationOutlinerTreeNode::EnterRenameMode()
{
	EditableLabel->EnterTextMode();
}


FLinearColor SAnimationOutlinerTreeNode::GetHoverTint() const
{
	return IsHovered() ? FLinearColor(1,1,1,0.9f) : FLinearColor(1,1,1,0.4f);
}


FReply SAnimationOutlinerTreeNode::OnPreviousKeyClicked()
{
	FSequencer& Sequencer = DisplayNode->GetSequencer();
	float ClosestPreviousKeyDistance = MAX_FLT;
	float CurrentTime = Sequencer.GetCurrentLocalTime(*Sequencer.GetFocusedMovieSceneSequence());
	float PreviousTime = 0;
	bool PreviousKeyFound = false;

	TSet<TSharedPtr<IKeyArea>> KeyAreas;
	SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);
	for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
	{
		for (FKeyHandle& KeyHandle : KeyArea->GetUnsortedKeyHandles())
		{
			float KeyTime = KeyArea->GetKeyTime(KeyHandle);
			if (KeyTime < CurrentTime && CurrentTime - KeyTime < ClosestPreviousKeyDistance)
			{
				PreviousTime = KeyTime;
				ClosestPreviousKeyDistance = CurrentTime - KeyTime;
				PreviousKeyFound = true;
			}
		}
	}

	if (PreviousKeyFound)
	{
		Sequencer.SetGlobalTime(PreviousTime);
	}
	return FReply::Handled();
}


FReply SAnimationOutlinerTreeNode::OnNextKeyClicked()
{
	FSequencer& Sequencer = DisplayNode->GetSequencer();
	float ClosestNextKeyDistance = MAX_FLT;
	float CurrentTime = Sequencer.GetCurrentLocalTime(*Sequencer.GetFocusedMovieSceneSequence());
	float NextTime = 0;
	bool NextKeyFound = false;

	TSet<TSharedPtr<IKeyArea>> KeyAreas;
	SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);
	for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
	{
		for (FKeyHandle& KeyHandle : KeyArea->GetUnsortedKeyHandles())
		{
			float KeyTime = KeyArea->GetKeyTime(KeyHandle);
			if (KeyTime > CurrentTime && KeyTime - CurrentTime < ClosestNextKeyDistance)
			{
				NextTime = KeyTime;
				ClosestNextKeyDistance = KeyTime - CurrentTime;
				NextKeyFound = true;
			}
		}
	}

	if (NextKeyFound)
	{
		Sequencer.SetGlobalTime(NextTime);
	}

	return FReply::Handled();
}


FReply SAnimationOutlinerTreeNode::OnAddKeyClicked()
{
	FSequencer& Sequencer = DisplayNode->GetSequencer();
	float CurrentTime = Sequencer.GetCurrentLocalTime(*Sequencer.GetFocusedMovieSceneSequence());

	TSet<TSharedPtr<IKeyArea>> KeyAreas;
	SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);

	TArray<UMovieSceneSection*> KeyAreaSections;
	for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
	{
		UMovieSceneSection* OwningSection = KeyArea->GetOwningSection();
		KeyAreaSections.Add(OwningSection);
	}

	UMovieSceneSection* NearestSection = MovieSceneHelpers::FindNearestSectionAtTime(KeyAreaSections, CurrentTime);
	if (!NearestSection)
	{
		return FReply::Unhandled();
	}

	FScopedTransaction Transaction(LOCTEXT("AddKeys", "Add keys at current time"));
	for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
	{
		UMovieSceneSection* OwningSection = KeyArea->GetOwningSection();
		if (OwningSection == NearestSection)
		{
			OwningSection->SetFlags(RF_Transactional);
			if (OwningSection->TryModify())
			{
				KeyArea->AddKeyUnique(CurrentTime, Sequencer.GetKeyInterpolation());
			}
		}
	}

	return FReply::Handled();
}


TSharedPtr<FSequencerDisplayNode> SAnimationOutlinerTreeNode::GetRootNode(TSharedPtr<FSequencerDisplayNode> ObjectNode)
{
	if (!ObjectNode->GetParent().IsValid())
	{
		return ObjectNode;
	}

	return GetRootNode(ObjectNode->GetParent());
}


void SAnimationOutlinerTreeNode::GetAllDescendantNodes(TSharedPtr<FSequencerDisplayNode> RootNode, TArray<TSharedRef<FSequencerDisplayNode> >& AllNodes)
{
	if (!RootNode.IsValid())
	{
		return;
	}

	AllNodes.Add(RootNode.ToSharedRef());

	const FSequencerDisplayNode* RootNodeC = RootNode.Get();

	for (TSharedRef<FSequencerDisplayNode> ChildNode : RootNodeC->GetChildNodes())
	{
		AllNodes.Add(ChildNode);
		GetAllDescendantNodes(ChildNode, AllNodes);
	}
}


FReply SAnimationOutlinerTreeNode::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (!DisplayNode->IsSelectable())
	{
		return FReply::Unhandled();
	}

	if ((MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) &&
		(MouseEvent.GetEffectingButton() != EKeys::RightMouseButton))
	{
		return FReply::Unhandled();
	}

	FSequencer& Sequencer = DisplayNode->GetSequencer();
	bool bSelected = Sequencer.GetSelection().IsSelected(DisplayNode.ToSharedRef());

	TArray<TSharedPtr<FSequencerDisplayNode> > AffectedNodes;
	AffectedNodes.Add(DisplayNode.ToSharedRef());

	Sequencer.GetSelection().SuspendBroadcast();

	if (MouseEvent.IsShiftDown())
	{
		FSequencerNodeTree& ParentTree = DisplayNode->GetParentTree();

		const TArray< TSharedRef<FSequencerDisplayNode> >RootNodes = ParentTree.GetRootNodes();

		// Get all nodes in order
		TArray<TSharedRef<FSequencerDisplayNode> > AllNodes;
		for (int32 i = 0; i < RootNodes.Num(); ++i)
		{
			GetAllDescendantNodes(RootNodes[i], AllNodes);
		}

		int32 FirstIndexToSelect = INT32_MAX;
		int32 LastIndexToSelect = INT32_MIN;

		for (int32 ChildIndex = 0; ChildIndex < AllNodes.Num(); ++ChildIndex)
		{
			TSharedRef<FSequencerDisplayNode> ChildNode = AllNodes[ChildIndex];

			if (ChildNode == DisplayNode.ToSharedRef() || Sequencer.GetSelection().IsSelected(ChildNode))
			{
				if (ChildIndex < FirstIndexToSelect)
				{
					FirstIndexToSelect = ChildIndex;
				}

				if (ChildIndex > LastIndexToSelect)
				{
					LastIndexToSelect = ChildIndex;
				}
			}
		}

		if (FirstIndexToSelect != INT32_MAX && LastIndexToSelect != INT32_MIN)
		{
			for (int32 ChildIndex = FirstIndexToSelect; ChildIndex <= LastIndexToSelect; ++ChildIndex)
			{
				TSharedRef<FSequencerDisplayNode> ChildNode = AllNodes[ChildIndex];

				if (!Sequencer.GetSelection().IsSelected(ChildNode))
				{
					Sequencer.GetSelection().AddToSelection(ChildNode);
					AffectedNodes.Add(ChildNode);
				}
			}
		}
	}
	else if( MouseEvent.IsControlDown() )
	{
		// Toggle selection when control is down
		if (bSelected)
		{
			Sequencer.GetSelection().RemoveFromSelection(DisplayNode.ToSharedRef());
		}
		else
		{
			Sequencer.GetSelection().AddToSelection(DisplayNode.ToSharedRef());
		}
	}
	else
	{
		// Deselect the other nodes and select this node.
		Sequencer.GetSelection().EmptySelectedOutlinerNodes();
		Sequencer.GetSelection().AddToSelection(DisplayNode.ToSharedRef());
	}

	OnSelectionChanged( AffectedNodes );

	Sequencer.GetSelection().ResumeBroadcast();
	Sequencer.GetSelection().GetOnOutlinerNodeSelectionChanged().Broadcast();

	return FReply::Handled();
}


FReply SAnimationOutlinerTreeNode::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.GetEffectingButton() != EKeys::RightMouseButton )
	{
		return FReply::Unhandled();
	}

	TSharedPtr<SWidget> MenuContent = DisplayNode->OnSummonContextMenu(MyGeometry, MouseEvent);

	if (!MenuContent.IsValid())
	{
		return FReply::Handled();
	}

	FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

	FSlateApplication::Get().PushMenu(
		AsShared(),
		WidgetPath,
		MenuContent.ToSharedRef(),
		MouseEvent.GetScreenSpacePosition(),
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
	);
			
	return FReply::Handled().SetUserFocus(MenuContent.ToSharedRef(), EFocusCause::SetDirectly);
}


const FSlateBrush* SAnimationOutlinerTreeNode::GetNodeBorderImage() const
{
	// Display a highlight when the node is selected
	FSequencer& Sequencer = DisplayNode->GetSequencer();
	const bool bIsSelected = Sequencer.GetSelection().IsSelected(DisplayNode.ToSharedRef());

	if (!bIsSelected)
	{
		return NotSelectedBrush;
	}

	if (Sequencer.GetSelection().GetActiveSelection() == FSequencerSelection::EActiveSelection::OutlinerNode)
	{
		return SelectedBrush;
	}

	return SelectedBrushInactive;
}


FSlateColor SAnimationOutlinerTreeNode::GetForegroundBasedOnSelection() const
{
	FSequencer& Sequencer = DisplayNode->GetSequencer();
	const bool bIsSelected = Sequencer.GetSelection().IsSelected(DisplayNode.ToSharedRef());

	return bIsSelected ? TableRowStyle->SelectedTextColor : TableRowStyle->TextColor;
}


EVisibility SAnimationOutlinerTreeNode::GetExpanderVisibility() const
{
	return DisplayNode->GetNumChildren() > 0 ? EVisibility::Visible : EVisibility::Hidden;
}


FText SAnimationOutlinerTreeNode::GetDisplayName() const
{
	return DisplayNode->GetDisplayName();
}


bool SAnimationOutlinerTreeNode::HandleNodeLabelCanEdit() const
{
	return DisplayNode->CanRenameNode();
}


void SAnimationOutlinerTreeNode::HandleNodeLabelTextChanged(const FText& NewLabel)
{
	DisplayNode->SetDisplayName(NewLabel);
}


void SAnimationOutlinerTreeNode::OnSelectionChanged( TArray<TSharedPtr<FSequencerDisplayNode> > AffectedNodes )
{
	FSequencer& Sequencer = DisplayNode->GetSequencer();
	if (!Sequencer.IsLevelEditorSequencer())
	{
		return;
	}

	TArray<TSharedPtr<FSequencerDisplayNode> > AffectedRootNodes;

	for (int32 NodeIdx = 0; NodeIdx < AffectedNodes.Num(); ++NodeIdx)
	{
		TSharedPtr<FSequencerDisplayNode> RootNode = GetRootNode(AffectedNodes[NodeIdx]);
		if (RootNode.IsValid() && RootNode->GetType() == ESequencerNode::Object)
		{
			AffectedRootNodes.Add(RootNode);
		}
	}

	if (!AffectedRootNodes.Num())
	{
		return;
	}

	const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
	bool IsControlDown = ModifierKeys.IsControlDown();
	bool IsShiftDown = ModifierKeys.IsShiftDown();

	TArray<AActor*> ActorsToSelect;
	TArray<AActor*> ActorsToRemainSelected;
	TArray<AActor*> ActorsToDeselect;

	// Find objects bound to the object node and determine what actors need to be selected in the editor
	for (int32 ObjectIdx = 0; ObjectIdx < AffectedRootNodes.Num(); ++ObjectIdx)
	{
		const TSharedPtr<const FSequencerObjectBindingNode> ObjectNode = StaticCastSharedPtr<const FSequencerObjectBindingNode>( AffectedRootNodes[ObjectIdx] );

		// Get the bound objects
		TArray<UObject*> RuntimeObjects;
		Sequencer.GetRuntimeObjects( Sequencer.GetFocusedMovieSceneSequenceInstance(), ObjectNode->GetObjectBinding(), RuntimeObjects );
		
		if( RuntimeObjects.Num() > 0 )
		{
			for( int32 ActorIdx = 0; ActorIdx < RuntimeObjects.Num(); ++ActorIdx )
			{
				AActor* Actor = Cast<AActor>( RuntimeObjects[ActorIdx] );

				if (Actor == nullptr)
				{
					continue;
				}

				bool bIsActorSelected = GEditor->GetSelectedActors()->IsSelected(Actor);
				if (IsControlDown)
				{
					if (!bIsActorSelected)
					{
						ActorsToSelect.Add(Actor);
					}
					else
					{
						ActorsToDeselect.Add(Actor);
					}
				}
				else
				{
					if (!bIsActorSelected)
					{
						ActorsToSelect.Add(Actor);
					}
					else
					{
						ActorsToRemainSelected.Add(Actor);
					}
				}
			}
		}
	}

	if (!IsControlDown && !IsShiftDown)
	{
		for (FSelectionIterator SelectionIt(*GEditor->GetSelectedActors()); SelectionIt; ++SelectionIt)
		{
			AActor* Actor = CastChecked< AActor >( *SelectionIt );
			if (Actor)
			{
				if (ActorsToSelect.Find(Actor) == INDEX_NONE && 
					ActorsToDeselect.Find(Actor) == INDEX_NONE &&
					ActorsToRemainSelected.Find(Actor) == INDEX_NONE)
				{
					ActorsToDeselect.Add(Actor);
				}
			}
		}
	}

	// If there's no selection to change, return early
	if (ActorsToSelect.Num() + ActorsToDeselect.Num() == 0)
	{
		return;
	}

	// Mark that the user is selecting so that the UI doesn't respond to the selection changes in the following block
	TSharedRef<SSequencer> SequencerWidget = StaticCastSharedRef<SSequencer>(Sequencer.GetSequencerWidget());
	SequencerWidget->SetUserIsSelecting(true);

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ClickingOnActors", "Clicking on Actors"));

	const bool bNotifySelectionChanged = false;
	const bool bDeselectBSP = true;
	const bool bWarnAboutTooManyActors = false;
	const bool bSelectEvenIfHidden = true;

	GEditor->GetSelectedActors()->Modify();

	if (!IsControlDown && !IsShiftDown)
	{
		GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
	}

	GEditor->GetSelectedActors()->BeginBatchSelectOperation();

	for (auto ActorToSelect : ActorsToSelect)
	{
		GEditor->SelectActor(ActorToSelect, true, bNotifySelectionChanged, bSelectEvenIfHidden);
	}

	for (auto ActorToDeselect : ActorsToDeselect)
	{
		GEditor->SelectActor(ActorToDeselect, false, bNotifySelectionChanged, bSelectEvenIfHidden);
	}

	GEditor->GetSelectedActors()->EndBatchSelectOperation();
	GEditor->NoteSelectionChange();

	// Unlock the selection so that the sequencer widget can now respond to selection changes in the level
	SequencerWidget->SetUserIsSelecting(false);
}


#undef LOCTEXT_NAMESPACE
