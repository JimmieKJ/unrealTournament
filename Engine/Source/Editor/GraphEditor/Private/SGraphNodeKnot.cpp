// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "SGraphNodeKnot.h"
#include "ScopedTransaction.h"
#include "BlueprintEditorUtils.h"
#include "GenericCommands.h"
#include "SInlineEditableTextBlock.h"

namespace SKnotNodeDefinitions
{
	/** Offset from the left edge to display comment toggle button at */
	static const float KnotCenterButtonAdjust = 3.f;

	/** Offset from the left edge to display comment bubbles at */
	static const float KnotCenterBubbleAdjust = 20.f;

	/** Knot node spacer sizes */
	static const FVector2D NodeSpacerSize(42.0f, 24.0f);
}

class FAmbivalentDirectionDragConnection : public FDragConnection
{
public:
	// FDragDropOperation interface
	virtual void OnDragged(const class FDragDropEvent& DragDropEvent) override;
	// End of FDragDropOperation interface

	UEdGraphPin* GetBestPin() const;

	// FDragConnection interface
	virtual void ValidateGraphPinList(TArray<UEdGraphPin*>& OutValidPins) override;
	// End of FDragConnection interface


	static TSharedRef<FAmbivalentDirectionDragConnection> New(UK2Node_Knot* InKnot, const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins, bool bInShiftOperation)
	{
		TSharedRef<FAmbivalentDirectionDragConnection> Operation = MakeShareable(new FAmbivalentDirectionDragConnection(InKnot, InGraphPanel, InStartingPins, bInShiftOperation));
		Operation->Construct();

		return Operation;
	}

protected:
	FAmbivalentDirectionDragConnection(UK2Node_Knot* InKnot, const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins, bool bInShiftOperation)
		: FDragConnection(InGraphPanel, InStartingPins, bInShiftOperation)
		, KnotPtr(InKnot)
		, StartScreenPos(FVector2D::ZeroVector)
		, MostRecentScreenPos(FVector2D::ZeroVector)
		, bLatchedStartScreenPos(false)
	{
	}

protected:
	TWeakObjectPtr<UK2Node_Knot> KnotPtr;

	FVector2D StartScreenPos;
	FVector2D MostRecentScreenPos;
	bool bLatchedStartScreenPos;
};

UEdGraphPin* FAmbivalentDirectionDragConnection::GetBestPin() const
{
	if (bLatchedStartScreenPos)
	{
		if (UK2Node_Knot* Knot = KnotPtr.Get())
		{
			const bool bIsRight = MostRecentScreenPos.X >= StartScreenPos.X;
			return bIsRight ? Knot->GetOutputPin() : Knot->GetInputPin();
		}
	}

	return nullptr;
}

void FAmbivalentDirectionDragConnection::OnDragged(const class FDragDropEvent& DragDropEvent)
{
	if (bLatchedStartScreenPos)
	{
		const FVector2D LastScreenPos = MostRecentScreenPos;
		MostRecentScreenPos = DragDropEvent.GetScreenSpacePosition();

		// Switch directions on the preview connector as we cross from left to right of the starting drag point (or vis versa)
		const bool bWasRight = LastScreenPos.X >= StartScreenPos.X;
		const bool bIsRight = MostRecentScreenPos.X >= StartScreenPos.X;

		if (bWasRight ^ bIsRight)
		{
			GraphPanel->OnStopMakingConnection(/*bForceStop=*/ true);
			GraphPanel->OnBeginMakingConnection(GetBestPin());
		}
	}
	else
	{
		StartScreenPos = DragDropEvent.GetScreenSpacePosition();
		MostRecentScreenPos = StartScreenPos;
		bLatchedStartScreenPos = true;
	}

	FDragConnection::OnDragged(DragDropEvent);
}

void FAmbivalentDirectionDragConnection::ValidateGraphPinList(TArray<UEdGraphPin*>& OutValidPins)
{
	OutValidPins.Empty(StartingPins.Num());

	if (UK2Node_Knot* Knot = KnotPtr.Get())
	{
		bool bUseOutput = true;

		// Pick output or input based on if the drag op is currently to the left or to the right of the starting drag point
		if (bLatchedStartScreenPos)
		{
			bUseOutput = (StartScreenPos.X < MostRecentScreenPos.X);
		}

		if (UEdGraphPin* TargetPinObj = GetHoveredPin())
		{
// 			if (UK2Node_Knot* TargetKnot = Cast<UK2Node_Knot>(TargetPinObj->GetOwningNode()))
// 			{
// 				// The visible pin on a knot is always an output, so Rely on the direction matching; since the visible pin on another knot is always an output
// 			}
// 			else
			{
				// Dragging to another pin, pick the opposite direction as a source to maximize connection chances
				if (TargetPinObj->Direction == EGPD_Input)
				{
					bUseOutput = true;
				}
				else
				{
					bUseOutput = false;
				}
			}
		}

		// Switch the effective valid pin so it makes sense for the current drag context
		if (bUseOutput)
		{
			OutValidPins.Add(Knot->GetOutputPin());
		}
		else
		{
			OutValidPins.Add(Knot->GetInputPin());
		}
	}
	else
	{
		// Fall back to the default behavior
		FDragConnection::ValidateGraphPinList(OutValidPins);
	}
}

/////////////////////////////////////////////////////
// SGraphPinKnot

class SGraphPinKnot : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinKnot) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

	// SWidget interface
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	// End of SWidget interface

protected:
	// Begin SGraphPin interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	virtual TSharedRef<FDragDropOperation> SpawnPinDragEvent(const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins, bool bInShiftOperation) override;
	virtual FReply OnPinMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent) override;
	virtual FSlateColor GetPinColor() const override;
	// End SGraphPin interface
};

void SGraphPinKnot::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments().SideToSideMargin(0.0f), InPin);
}

void SGraphPinKnot::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (Operation.IsValid() && Operation->IsOfType<FDragConnection>())
	{
		TSharedPtr<FDragConnection> DragConnectionOp = StaticCastSharedPtr<FDragConnection>(Operation);

		TArray<UEdGraphPin*> ValidPins;
		DragConnectionOp->ValidateGraphPinList(/*out*/ ValidPins);
		
		if (ValidPins.Num() > 0)
		{
			if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(GraphPinObj->GetOwningNode()))
			{
				// Dragging to another pin, pick the opposite direction as a source to maximize connection chances
				UEdGraphPin* PinToHoverOver = (ValidPins[0]->Direction == EGPD_Input) ? Knot->GetOutputPin() : Knot->GetInputPin();
				DragConnectionOp->SetHoveredPin(PinToHoverOver);

				// Pins treat being dragged over the same as being hovered outside of drag and drop if they know how to respond to the drag action.
				SBorder::OnMouseEnter(MyGeometry, DragDropEvent);

				return;
			}
		}
	}

	SGraphPin::OnDragEnter(MyGeometry, DragDropEvent);
}

FSlateColor SGraphPinKnot::GetPinColor() const
{
	// Make ourselves transparent if we're the input, since we are underneath the output pin and would double-blend looking ugly
	return (GetPinObj()->Direction == EEdGraphPinDirection::EGPD_Input) ? FLinearColor::Transparent : SGraphPin::GetPinColor();
}

TSharedRef<SWidget>	SGraphPinKnot::GetDefaultValueWidget()
{
	return SNullWidget::NullWidget;
}

TSharedRef<FDragDropOperation> SGraphPinKnot::SpawnPinDragEvent(const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins, bool bInShiftOperation)
{
	TSharedRef<FAmbivalentDirectionDragConnection> Operation = FAmbivalentDirectionDragConnection::New(CastChecked<UK2Node_Knot>(GetPinObj()->GetOwningNode()), InGraphPanel, InStartingPins, bInShiftOperation);
	return Operation;
}

FReply SGraphPinKnot::OnPinMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (!GraphPinObj->bNotConnectable && IsEditingEnabled())
		{
			if (MouseEvent.IsAltDown())
			{
				// Normally break connections, but overloaded here to delete the node entirely
				const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());

				UK2Node_Knot* NodeToDelete = CastChecked<UK2Node_Knot>(GetPinObj()->GetOwningNode());
				UBlueprint* OwnerBlueprint = NodeToDelete->GetBlueprint();
				NodeToDelete->GetGraph()->Modify();

				FBlueprintEditorUtils::RemoveNode(OwnerBlueprint, NodeToDelete, /*bDontRecompile=*/ true);
				FBlueprintEditorUtils::MarkBlueprintAsModified(OwnerBlueprint);

				return FReply::Handled();
			}
			else if (MouseEvent.IsControlDown())
			{
				// Normally moves the connections from one pin to another, but moves the node instead since it's really representing a set of connections
				// Returning unhandled will cause the node behind us to catch it and move us
				return FReply::Unhandled();
			}
		}
	}

	return SGraphPin::OnPinMouseDown(SenderGeometry, MouseEvent);
}

//////////////////////////////////////////////////////////////////////////
// SGraphNodeKnot

void SGraphNodeKnot::Construct(const FArguments& InArgs, UK2Node_Knot* InKnot)
{
	SGraphNodeDefault::Construct(SGraphNodeDefault::FArguments().GraphNodeObj(InKnot));
}

void SGraphNodeKnot::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	//@TODO: Keyboard focus on edit doesn't work unless the node is visible, but the text is just the comment and it's already shown in a bubble, so Transparent black it is...
	InlineEditableText = SNew(SInlineEditableTextBlock)
		.ColorAndOpacity(FLinearColor::Transparent)
		.Style(FEditorStyle::Get(), "Graph.Node.NodeTitleInlineEditableText")
		.Text(this, &SGraphNodeKnot::GetEditableNodeTitleAsText)
		.OnVerifyTextChanged(this, &SGraphNodeKnot::OnVerifyNameTextChanged)
		.OnTextCommitted(this, &SGraphNodeKnot::OnNameTextCommited)
		.IsReadOnly(this, &SGraphNodeKnot::IsNameReadOnly)
		.IsSelected(this, &SGraphNodeKnot::IsSelectedExclusively);

	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );

	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				// Grab handle to be able to move the node
				SNew(SSpacer)
				.Size(SKnotNodeDefinitions::NodeSpacerSize)
				.Visibility(EVisibility::Visible)
				.Cursor(EMouseCursor::CardinalCross)
			]

			+SOverlay::Slot()
// 			.VAlign(VAlign_Center)
// 			.HAlign(HAlign_Center)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							// LEFT
							SAssignNew(LeftNodeBox, SVerticalBox)
						]
						+SOverlay::Slot()
						[
							// RIGHT
							SAssignNew(RightNodeBox, SVerticalBox)
						]
					]
				]
			]
		];
	// Create comment bubble
	const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

	SAssignNew(CommentBubble, SCommentBubble)
	.GraphNode(GraphNode)
	.Text(this, &SGraphNode::GetNodeComment)
	.OnTextCommitted(this, &SGraphNode::OnCommentTextCommitted)
	.EnableTitleBarBubble(true)
	.EnableBubbleCtrls(true)
	.AllowPinning(true)
	.ColorAndOpacity(CommentColor)
	.GraphLOD(this, &SGraphNode::GetCurrentLOD)
	.IsGraphNodeHovered(this, &SGraphNode::IsHovered)
	.OnToggled(this, &SGraphNode::OnCommentBubbleToggled);

	GetOrAddSlot(ENodeZone::TopCenter)
	.SlotOffset(TAttribute<FVector2D>(this, &SGraphNodeKnot::GetCommentOffset))
	.SlotSize( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetSize))
	.AllowScaling( TAttribute<bool>(CommentBubble.Get(), &SCommentBubble::IsScalingAllowed))
	.VAlign(VAlign_Top)
	[
		CommentBubble.ToSharedRef()
	];
	CreatePinWidgets();
}

const FSlateBrush* SGraphNodeKnot::GetShadowBrush(bool bSelected) const
{
	return bSelected ? FEditorStyle::GetBrush(TEXT("Graph.Node.ShadowSelected")) : FEditorStyle::GetNoBrush();
}

TSharedPtr<SGraphPin> SGraphNodeKnot::CreatePinWidget(UEdGraphPin* Pin) const
{
	return SNew(SGraphPinKnot, Pin);
}

void SGraphNodeKnot::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));

	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();

	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		LeftNodeBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
//			.Padding(10, 4)
			[
				PinToAdd
			];
		InputPins.Add(PinToAdd);
	}
	else
	{
		RightNodeBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
//			.Padding(10, 4)
			[
				PinToAdd
			];
		OutputPins.Add(PinToAdd);
	}
}

FVector2D SGraphNodeKnot::GetCommentOffset() const
{
	const bool bBubbleVisible = GraphNode->bCommentBubbleVisible || bAlwaysShowCommentBubble;
	const float ZoomAmount = GraphNode->bCommentBubblePinned && OwnerGraphPanelPtr.IsValid() ? OwnerGraphPanelPtr.Pin()->GetZoomAmount() : 1.f;
	const float NodeWidthOffset = bBubbleVisible ?	SKnotNodeDefinitions::KnotCenterBubbleAdjust * ZoomAmount :
													SKnotNodeDefinitions::KnotCenterButtonAdjust * ZoomAmount;

	return FVector2D(NodeWidthOffset - CommentBubble->GetArrowCenterOffset(), -CommentBubble->GetDesiredSize().Y);
}

void SGraphNodeKnot::OnCommentBubbleToggled(bool bInCommentBubbleVisible)
{
	SGraphNode::OnCommentBubbleToggled(bInCommentBubbleVisible);
	bAlwaysShowCommentBubble = bInCommentBubbleVisible;
}

void SGraphNodeKnot::OnCommentTextCommitted(const FText& NewComment, ETextCommit::Type CommitInfo)
{
	SGraphNode::OnCommentTextCommitted(NewComment, CommitInfo);
	if (!bAlwaysShowCommentBubble && !CommentBubble->TextBlockHasKeyboardFocus() && !CommentBubble->IsHovered())
	{
		// Hide the comment bubble if visibility hasn't changed
		CommentBubble->SetCommentBubbleVisibility(/*bVisible =*/false);
	}
}

void SGraphNodeKnot::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SGraphNode::OnMouseEnter(MyGeometry, MouseEvent);
	if (!GraphNode->bCommentBubbleVisible && !GraphNode->NodeComment.IsEmpty())
	{
		// Show the bubble widget while hovered
		CommentBubble->SetCommentBubbleVisibility(/*bVisible =*/true);
	}
}

void SGraphNodeKnot::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SGraphNode::OnMouseLeave(MouseEvent);
	if (!bAlwaysShowCommentBubble && !CommentBubble->TextBlockHasKeyboardFocus())
	{
		// Hide the comment bubble if visibility hasn't changed;
		CommentBubble->SetCommentBubbleVisibility(/*bVisible =*/false);
	}
}