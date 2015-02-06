// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnvironmentQueryEditorPrivatePCH.h"
#include "SGraphPreviewer.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "NodeFactory.h"
#include "SGraphNode.h"
#include "SGraphNode_EnvironmentQuery.h"
#include "SGraphPin.h"
#include "SGraphPanel.h"
#include "ScopedTransaction.h"
#include "EnvironmentQueryColors.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "EnvironmentQueryEditor"

/////////////////////////////////////////////////////
// SStateMachineOutputPin

class SEnvironmentQueryPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SEnvironmentQueryPin){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);
protected:
	// Begin SGraphPin interface
	virtual FSlateColor GetPinColor() const override;
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	// End SGraphPin interface

	const FSlateBrush* GetPinBorder() const;
};

void SEnvironmentQueryPin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	this->SetCursor( EMouseCursor::Default );

	bShowLabel = true;
	IsEditable = true;

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	check(Schema);

	SBorder::Construct( SBorder::FArguments()
		.BorderImage( this, &SEnvironmentQueryPin::GetPinBorder )
		.BorderBackgroundColor( this, &SEnvironmentQueryPin::GetPinColor )
		.OnMouseButtonDown( this, &SEnvironmentQueryPin::OnPinMouseDown )
		.Cursor( this, &SEnvironmentQueryPin::GetPinCursor )
		.Padding(FMargin(5.0f))
		);
}

TSharedRef<SWidget>	SEnvironmentQueryPin::GetDefaultValueWidget()
{
	return SNew(STextBlock);
}

const FSlateBrush* SEnvironmentQueryPin::GetPinBorder() const
{
	return FEditorStyle::GetBrush(TEXT("Graph.StateNode.Body"));
}

FSlateColor SEnvironmentQueryPin::GetPinColor() const
{
	return IsHovered() ? EnvironmentQueryColors::Pin::Hover :
		EnvironmentQueryColors::Pin::Default;
}

/////////////////////////////////////////////////////
// SGraphNode_EnvironmentQuery

void SGraphNode_EnvironmentQuery::Construct(const FArguments& InArgs, UEnvironmentQueryGraphNode* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();

	bIsMouseDown = false;
	bDragMarkerVisible = false;
}

void SGraphNode_EnvironmentQuery::AddTest(TSharedPtr<SGraphNode> TestWidget)
{
	TestBox->AddSlot().AutoHeight()
		[
			TestWidget.ToSharedRef()
		];
	TestWidgets.Add(TestWidget);
}

FSlateColor SGraphNode_EnvironmentQuery::GetBorderBackgroundColor() const
{
	UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
	const bool bSelectedSubNode = TestNode && TestNode->ParentNode && GetOwnerPanel().IsValid() && GetOwnerPanel()->SelectionManager.SelectedNodes.Contains(GraphNode);

	return bSelectedSubNode ? EnvironmentQueryColors::NodeBorder::Selected :
		EnvironmentQueryColors::NodeBorder::Default;
}

FSlateColor SGraphNode_EnvironmentQuery::GetBackgroundColor() const
{
	const UEnvironmentQueryGraphNode* MyNode = Cast<UEnvironmentQueryGraphNode>(GraphNode);
	const UClass* MyClass = MyNode && MyNode->NodeInstance ? MyNode->NodeInstance->GetClass() : NULL;
	
	FLinearColor NodeColor = EnvironmentQueryColors::NodeBody::Default;
	if (MyClass)
	{
		if (MyClass->IsChildOf( UEnvQueryTest::StaticClass() ))
		{
			UEnvironmentQueryGraphNode_Test* MyTestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
			NodeColor = (MyTestNode && MyTestNode->bTestEnabled) ? EnvironmentQueryColors::NodeBody::Test : EnvironmentQueryColors::NodeBody::TestInactive;
		}
		else if (MyClass->IsChildOf( UEnvQueryOption::StaticClass() ))
		{
			NodeColor = EnvironmentQueryColors::NodeBody::Generator;
		}
	}

	return NodeColor;
}

void SGraphNode_EnvironmentQuery::UpdateGraphNode()
{
	if (TestBox.IsValid())
	{
		TestBox->ClearChildren();
	} 
	else
	{
		SAssignNew(TestBox,SVerticalBox);
	}

	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();
	TestWidgets.Reset();

	const float NodePadding = (Cast<UEnvironmentQueryGraphNode_Test>(GraphNode) != NULL) ? 2.0f : 10.0f;
	float TestPadding = 0.0f;

	UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(GraphNode);
	if (OptionNode)
	{
		for (int32 i = 0; i < OptionNode->Tests.Num(); i++)
		{
			if (OptionNode->Tests[i])
			{
				TSharedPtr<SGraphNode> NewNode = FNodeFactory::CreateNodeWidget(OptionNode->Tests[i]);
				if (OwnerGraphPanelPtr.IsValid())
				{
					NewNode->SetOwner(OwnerGraphPanelPtr.Pin().ToSharedRef());
					OwnerGraphPanelPtr.Pin()->AttachGraphEvents(NewNode);
				}
				AddTest(NewNode);
				NewNode->UpdateGraphNode();
			}
		}

		if (TestWidgets.Num() == 0)
		{
			TestBox->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("NoTests","Right click to add tests"))
				];
		}

		TestPadding = 10.0f;
	}

	const FSlateBrush* NodeTypeIcon = GetNameIcon();

	FLinearColor TitleShadowColor(1.0f, 0.0f, 0.0f);
	TSharedPtr<SErrorText> ErrorText;
	TSharedPtr<STextBlock> DescriptionText; 
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( "Graph.StateNode.Body" ) )
			.Padding(0)
			.BorderBackgroundColor( this, &SGraphNode_EnvironmentQuery::GetBorderBackgroundColor )
			.OnMouseButtonDown(this, &SGraphNode_EnvironmentQuery::OnMouseDown)
			.OnMouseButtonUp(this, &SGraphNode_EnvironmentQuery::OnMouseUp)
			[
				SNew(SOverlay)

				// INPUT PIN AREA
				+SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Top)
					[
						SAssignNew(LeftNodeBox, SVerticalBox)
					]

				// OUTPUT PIN AREA
				+SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					[
						SAssignNew(RightNodeBox, SVerticalBox)
					]

				// STATE NAME AREA
				+SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					.Padding(NodePadding)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SCheckBox)
							.Visibility(this, &SGraphNode_EnvironmentQuery::GetTestToggleVisibility)
							.IsChecked(this, &SGraphNode_EnvironmentQuery::IsTestToggleChecked)
							.OnCheckStateChanged(this, &SGraphNode_EnvironmentQuery::OnTestToggleChanged)
							.Padding(FMargin(0, 0, 4.0f, 0))
						]
						+SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBorder)
								.BorderImage( FEditorStyle::GetBrush("Graph.StateNode.Body") )
								.BorderBackgroundColor( this, &SGraphNode_EnvironmentQuery::GetBackgroundColor )
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Center)
								.Visibility(EVisibility::SelfHitTestInvisible)
								[
									SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.AutoHeight()
										.Padding(0,0,0,2)
										[
											SNew(SBox).HeightOverride(4)
											[
												// weight bar
												SNew(SProgressBar)
												.FillColorAndOpacity(this, &SGraphNode_EnvironmentQuery::GetWeightProgressBarColor)
												.Visibility(this, &SGraphNode_EnvironmentQuery::GetWeightMarkerVisibility)
												.Percent(this, &SGraphNode_EnvironmentQuery::GetWeightProgressBarPercent)
											]
										]
										+SVerticalBox::Slot()
										.AutoHeight()
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
													.AutoWidth()
													[
														// POPUP ERROR MESSAGE
														SAssignNew(ErrorText, SErrorText )
														.BackgroundColor( this, &SGraphNode_EnvironmentQuery::GetErrorColor )
														.ToolTipText( this, &SGraphNode_EnvironmentQuery::GetErrorMsgToolTip )
													]
												+SHorizontalBox::Slot()
													.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
													[
														SNew(SVerticalBox)
														+SVerticalBox::Slot()
														.AutoHeight()
															[
																SAssignNew(InlineEditableText, SInlineEditableTextBlock)
																.Style( FEditorStyle::Get(), "Graph.StateNode.NodeTitleInlineEditableText" )
																.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
																.OnVerifyTextChanged(this, &SGraphNode_EnvironmentQuery::OnVerifyNameTextChanged)
																.OnTextCommitted(this, &SGraphNode_EnvironmentQuery::OnNameTextCommited)
																.IsReadOnly( this, &SGraphNode_EnvironmentQuery::IsNameReadOnly )
																.IsSelected(this, &SGraphNode_EnvironmentQuery::IsSelectedExclusively)
															]
														+SVerticalBox::Slot()
															.AutoHeight()
															[
																NodeTitle.ToSharedRef()
															]
													]
											]
										+SVerticalBox::Slot()
											.AutoHeight()
											[
												// DESCRIPTION MESSAGE
												SAssignNew(DescriptionText, STextBlock )
												.Text(this, &SGraphNode_EnvironmentQuery::GetDescription)
											]
								]
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0, TestPadding, 0, 0)
							[
								TestBox.ToSharedRef()
							]
						]
					]
				//drag marker overlay
				+SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Top)
					[
						SNew(SBorder)
						.BorderBackgroundColor(EnvironmentQueryColors::Action::DragMarker)
						.ColorAndOpacity(EnvironmentQueryColors::Action::DragMarker)
						.BorderImage(FEditorStyle::GetBrush("Graph.StateNode.Body"))
						.Visibility(this, &SGraphNode_EnvironmentQuery::GetDragOverMarkerVisibility)
						[
							SNew(SBox)
							.HeightOverride(4)
						]
					]
			]
		];

	ErrorReporting = ErrorText;
	ErrorReporting->SetError(ErrorMsg);
	CreatePinWidgets();
}

FText SGraphNode_EnvironmentQuery::GetDescription() const
{
	UEnvironmentQueryGraphNode* StateNode = CastChecked<UEnvironmentQueryGraphNode>(GraphNode);
	return StateNode ? StateNode->GetDescription() : FText::GetEmpty();
}

void SGraphNode_EnvironmentQuery::CreatePinWidgets()
{
	UEnvironmentQueryGraphNode* StateNode = CastChecked<UEnvironmentQueryGraphNode>(GraphNode);

	UEdGraphPin* CurPin = StateNode->GetOutputPin();
	if (CurPin && !CurPin->bHidden)
	{
		TSharedPtr<SGraphPin> NewPin = SNew(SEnvironmentQueryPin, CurPin);

		NewPin->SetIsEditable(IsEditable);

		this->AddPin(NewPin.ToSharedRef());
	}

	CurPin = StateNode->GetInputPin();
	if (CurPin && !CurPin->bHidden)
	{
		TSharedPtr<SGraphPin> NewPin = SNew(SEnvironmentQueryPin, CurPin);

		NewPin->SetIsEditable(IsEditable);

		this->AddPin(NewPin.ToSharedRef());
	}
}

void SGraphNode_EnvironmentQuery::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner( SharedThis(this) );

	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
	const bool bAdvancedParameter = PinObj && PinObj->bAdvancedView;
	if(bAdvancedParameter)
	{
		PinToAdd->SetVisibility( TAttribute<EVisibility>(PinToAdd, &SGraphPin::IsPinVisibleAsAdvanced) );
	}

	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		LeftNodeBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			[
				PinToAdd
			];
		InputPins.Add(PinToAdd);
	}
	else // Direction == EEdGraphPinDirection::EGPD_Output
	{
		RightNodeBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			[
				PinToAdd
			];
		OutputPins.Add(PinToAdd);
	}
}

void SGraphNode_EnvironmentQuery::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (bIsMouseDown)
	{
		MouseDownTime += InDeltaTime;
	}
}

FReply SGraphNode_EnvironmentQuery::OnMouseMove(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent)
{
	if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && bIsMouseDown)
	{
		bIsMouseDown = false;
		MouseDownTime = 0;
	}

	if (bIsMouseDown && MouseDownTime > 0.1f)
	{
		//if we are holding mouse over a subnode
		UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
		if (TestNode && TestNode->ParentNode)
		{
			const TSharedRef<SGraphPanel>& Panel = this->GetOwnerPanel().ToSharedRef();
			const TSharedRef<SGraphNode>& Node = SharedThis(this);
			return FReply::Handled().BeginDragDrop(FDragNode::New(Panel, Node));
		}
	}
	return FReply::Unhandled();
}

FReply SGraphNode_EnvironmentQuery::OnMouseUp(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent)
{
	bIsMouseDown = false;
	MouseDownTime = 0;
	return FReply::Unhandled();
}

FReply SGraphNode_EnvironmentQuery::OnMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent)
{
	bIsMouseDown = true;

	UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
	if (TestNode && TestNode->ParentNode)
	{
		GetOwnerPanel()->SelectionManager.ClickedOnNode(GraphNode,MouseEvent);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedPtr<SGraphNode> SGraphNode_EnvironmentQuery::GetSubNodeUnderCursor(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SGraphNode> ResultNode;

	// We just need to find the one WidgetToFind among our descendants.
	TSet< TSharedRef<SWidget> > SubWidgetsSet;
	for (int32 i=0; i < TestWidgets.Num(); i++)
	{
		SubWidgetsSet.Add(TestWidgets[i].ToSharedRef());
	}

	TMap<TSharedRef<SWidget>, FArrangedWidget> Result;
	FindChildGeometries(WidgetGeometry, SubWidgetsSet, Result);

	if ( Result.Num() > 0 )
	{
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		Result.GenerateValueArray( ArrangedChildren.GetInternalArray() );
		int32 HoveredIndex = SWidget::FindChildUnderMouse( ArrangedChildren, MouseEvent );
		if ( HoveredIndex != INDEX_NONE )
		{
			ResultNode = StaticCastSharedRef<SGraphNode>(ArrangedChildren[HoveredIndex].Widget);
		}
	}

	return ResultNode;
}

void SGraphNode_EnvironmentQuery::SetDragMarker(bool bEnabled)
{
	bDragMarkerVisible = bEnabled;
}

EVisibility SGraphNode_EnvironmentQuery::GetDragOverMarkerVisibility() const
{
	return bDragMarkerVisible ? EVisibility::Visible : EVisibility::Collapsed; 
}

EVisibility SGraphNode_EnvironmentQuery::GetWeightMarkerVisibility() const
{
	UEnvironmentQueryGraphNode_Test* MyTestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
	return MyTestNode ? EVisibility::Visible : EVisibility::Collapsed;
}

TOptional<float> SGraphNode_EnvironmentQuery::GetWeightProgressBarPercent() const
{
	UEnvironmentQueryGraphNode_Test* MyTestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
	return MyTestNode ? FMath::Max(0.0f, MyTestNode->TestWeightPct) : TOptional<float>();
}

FSlateColor SGraphNode_EnvironmentQuery::GetWeightProgressBarColor() const
{
	UEnvironmentQueryGraphNode_Test* MyTestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
	return (MyTestNode && MyTestNode->bHasNamedWeight) ? EnvironmentQueryColors::Action::WeightNamed : EnvironmentQueryColors::Action::Weight;
}

EVisibility SGraphNode_EnvironmentQuery::GetTestToggleVisibility() const
{
	UEnvironmentQueryGraphNode_Test* MyTestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
	return MyTestNode ? EVisibility::Visible : EVisibility::Collapsed;
}

ECheckBoxState SGraphNode_EnvironmentQuery::IsTestToggleChecked() const
{
	UEnvironmentQueryGraphNode_Test* MyTestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
	return MyTestNode && MyTestNode->bTestEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SGraphNode_EnvironmentQuery::OnTestToggleChanged(ECheckBoxState NewState)
{
	UEnvironmentQueryGraphNode_Test* MyTestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
	if (MyTestNode)
	{
		MyTestNode->bTestEnabled = (NewState == ECheckBoxState::Checked);
		
		if (MyTestNode->ParentNode)
		{
			MyTestNode->ParentNode->CalculateWeights();
		}

		UEnvironmentQueryGraph* MyGraph = Cast<UEnvironmentQueryGraph>(MyTestNode->GetGraph());
		if (MyGraph)
		{
			MyGraph->UpdateAsset();
		}
	}
}

void SGraphNode_EnvironmentQuery::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// Is someone dragging a node?
	TSharedPtr<FDragNode> DragConnectionOp = DragDropEvent.GetOperationAs<FDragNode>();
	if (DragConnectionOp.IsValid())
	{
		// Inform the Drag and Drop operation that we are hovering over this node.
		TSharedPtr<SGraphNode> SubNode = GetSubNodeUnderCursor(MyGeometry, DragDropEvent);
		DragConnectionOp->SetHoveredNode( SubNode.IsValid() ? SubNode : SharedThis(this) );

		UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(GraphNode);
		if (DragConnectionOp->IsValidOperation() && TestNode && TestNode->ParentNode)
		{
			SetDragMarker(true);
		}
	}

	SGraphNode::OnDragEnter(MyGeometry, DragDropEvent);
}

FReply SGraphNode_EnvironmentQuery::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// Is someone dragging a node?
	TSharedPtr<FDragNode> DragConnectionOp = DragDropEvent.GetOperationAs<FDragNode>();
	if (DragConnectionOp.IsValid())
	{
		// Inform the Drag and Drop operation that we are hovering over this node.
		TSharedPtr<SGraphNode> SubNode = GetSubNodeUnderCursor(MyGeometry, DragDropEvent);
		DragConnectionOp->SetHoveredNode( SubNode.IsValid() ? SubNode : SharedThis(this) );
	}
	return SGraphNode::OnDragOver(MyGeometry, DragDropEvent);
}

void SGraphNode_EnvironmentQuery::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragNode> DragConnectionOp = DragDropEvent.GetOperationAs<FDragNode>();
	if (DragConnectionOp.IsValid())
	{
		// Inform the Drag and Drop operation that we are not hovering any pins
		DragConnectionOp->SetHoveredNode( TSharedPtr<SGraphNode>(NULL) );
	}
	SetDragMarker(false);

	SGraphNode::OnDragLeave(DragDropEvent);
}

FReply SGraphNode_EnvironmentQuery::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragNode> DragNodeOp = DragDropEvent.GetOperationAs<FDragNode>();
	if (DragNodeOp.IsValid())
	{
		if (!DragNodeOp->IsValidOperation())
		{
			return FReply::Handled();
		}

		UEnvironmentQueryGraphNode_Option* MyNode = Cast<UEnvironmentQueryGraphNode_Option>(GraphNode);
		if (MyNode == NULL)
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_DragDropNode", "Drag&Drop Node") );
		bool bReorderOperation = true;

		const TArray< TSharedRef<SGraphNode> >& DraggedNodes = DragNodeOp->GetNodes();
		for (int32 i = 0; i < DraggedNodes.Num(); i++)
		{
			UEnvironmentQueryGraphNode_Test* DraggedTestNode = Cast<UEnvironmentQueryGraphNode_Test>(DraggedNodes[i]->GetNodeObj());
			if (DraggedTestNode && DraggedTestNode->ParentNode)
			{
				if (DraggedTestNode->ParentNode != GraphNode)
				{
					bReorderOperation = false;
				}

				DraggedTestNode->ParentNode->Modify();
				TArray<UEnvironmentQueryGraphNode_Test*> & SubNodes = DraggedTestNode->ParentNode->Tests;
				for (int32 j = 0; j < SubNodes.Num(); j++)
				{
					if (SubNodes[j] == DraggedTestNode)
					{
						SubNodes.RemoveAt(j);
					}
				}
			}
		}

		int32 InsertIndex = 0;
		for (int32 i=0; i < TestWidgets.Num(); i++)
		{
			TSharedPtr<SGraphNode_EnvironmentQuery> TestNodeWidget = StaticCastSharedPtr<SGraphNode_EnvironmentQuery>(TestWidgets[i]);
			if (TestNodeWidget->bDragMarkerVisible)
			{
				InsertIndex = MyNode->Tests.IndexOfByKey(TestNodeWidget->GetNodeObj());
				break;
			}
		}

		for (int32 i = 0; i < DraggedNodes.Num(); i++)
		{
			UEnvironmentQueryGraphNode_Test* DraggedTestNode = Cast<UEnvironmentQueryGraphNode_Test>(DraggedNodes[i]->GetNodeObj());
			DraggedTestNode->Modify();
			DraggedTestNode->ParentNode = MyNode;
			MyNode->Modify();
			MyNode->Tests.Insert(DraggedTestNode, InsertIndex);
		}

		if (bReorderOperation)
		{
			UpdateGraphNode();
		}
		else
		{
			MyNode->GetGraph()->NotifyGraphChanged();
		}
	}

	return SGraphNode::OnDrop(MyGeometry, DragDropEvent);
}

TSharedPtr<SToolTip> SGraphNode_EnvironmentQuery::GetComplexTooltip()
{
	return NULL;
}

FText SGraphNode_EnvironmentQuery::GetPreviewCornerText() const
{
	UEnvironmentQueryGraphNode* StateNode = CastChecked<UEnvironmentQueryGraphNode>(GraphNode);
	return FText::FromString(TEXT("Test CornerText"));
}

const FSlateBrush* SGraphNode_EnvironmentQuery::GetNameIcon() const
{
	return FEditorStyle::GetBrush( TEXT("Graph.StateNode.Icon") );
}

void SGraphNode_EnvironmentQuery::SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel)
{
	SGraphNode::SetOwner(OwnerPanel);

	for (auto& ChildWidget : TestWidgets)
	{
		if (ChildWidget.IsValid())
		{
			ChildWidget->SetOwner(OwnerPanel);
		}
	}
}

#undef LOCTEXT_NAMESPACE
