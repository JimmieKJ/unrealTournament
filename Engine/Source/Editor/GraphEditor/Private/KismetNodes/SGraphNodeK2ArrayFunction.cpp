// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SGraphNodeK2Base.h"
#include "SGraphNodeK2ArrayFunction.h"
#include "KismetPins/SGraphPinExec.h"
#include "NodeFactory.h"

#include "ScopedTransaction.h"
#include "BlueprintEditorUtils.h"

//////////////////////////////////////////////////////////////////////////
// SGraphNodeSwitchStatement

void SGraphNodeK2ArrayFunction::Construct(const FArguments& InArgs, UK2Node_CallArrayFunction* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor( EMouseCursor::CardinalCross );

	this->UpdateGraphNode();
}

FOptionalSize SGraphNodeK2ArrayFunction::GetBackgroundImageSize() const
{
	return MainNodeContent.Pin()->GetDesiredSize().Y - 8.0f;
}

FSlateColor SGraphNodeK2ArrayFunction::GetTypeIconColor() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UK2Node_CallArrayFunction* CallNode = CastChecked<UK2Node_CallArrayFunction>(GraphNode);
	FLinearColor Color = K2Schema->GetPinTypeColor(CallNode->GetTargetArrayPin()->PinType);
	Color.A = 0.25f;
	return Color;
}

void SGraphNodeK2ArrayFunction::UpdateGraphNode()
{
	UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);
	const bool bCompactMode = K2Node->ShouldDrawCompact();


	if(!bCompactMode)
	{
		SGraphNodeK2Base::UpdateGraphNode();
	}
	else
	{
		InputPins.Empty();
		OutputPins.Empty();

		// error handling set-up
		SetupErrorReporting();

		// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
		RightNodeBox.Reset();
		LeftNodeBox.Reset();

		//
		//             ______________________
		//            | (>) L |      | R (>) |
		//            | (>) E |      | I (>) |
		//            | (>) F |   +  | G (>) |
		//            | (>) T |      | H (>) |
		//            |       |      | T (>) |
		//            |_______|______|_______|
		//
		TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode)
			.Text(this, &SGraphNodeK2ArrayFunction::GetNodeCompactTitle);

		this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
		this->GetOrAddSlot( ENodeZone::Center )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
					[
						// NODE CONTENT AREA
						SAssignNew(MainNodeContent, SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image( FEditorStyle::GetBrush("Graph.VarNode.Body") )
						]
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Image( FEditorStyle::GetBrush("Graph.VarNode.Gloss") )
						]
						+SOverlay::Slot()
							.Padding( FMargin(0,3) )
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.Padding( FMargin(0,0,5,0) )
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								.AutoWidth()
								//.FillWidth(1.0f)
								[
									// LEFT
									SAssignNew(LeftNodeBox, SVerticalBox)
								]

								+SHorizontalBox::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(SOverlay)
										+SOverlay::Slot()
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											[
												SNew(SBox)
													.WidthOverride(this, &SGraphNodeK2ArrayFunction::GetBackgroundImageSize)
													.HeightOverride(this, &SGraphNodeK2ArrayFunction::GetBackgroundImageSize)
													[
														SNew(SImage)
														//.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.15f))
														.ColorAndOpacity(this, &SGraphNodeK2ArrayFunction::GetTypeIconColor)
														.Image( FEditorStyle::GetBrush(TEXT("Kismet.VariableList.ArrayTypeIcon")) )
													]
											]
										+SOverlay::Slot()
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											[
												// MIDDLE
												SNew(SVerticalBox)
												+SVerticalBox::Slot()
													.HAlign(HAlign_Center)
													.AutoHeight()
												[
													SNew(STextBlock)
														.TextStyle( FEditorStyle::Get(), "Graph.ArrayCompactNode.Title" )
														.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
														.WrapTextAt(128.0f)
												]
												+SVerticalBox::Slot()
													.AutoHeight()
												[
													NodeTitle.ToSharedRef()
												]
											]
									]
								+SHorizontalBox::Slot()
									.AutoWidth()
									.Padding( FMargin(5,0,0,0) )
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Center)
									[
										// RIGHT
										SAssignNew(RightNodeBox, SVerticalBox)
									]
							]
					]
				+SVerticalBox::Slot()
					.AutoHeight()
					.Padding( FMargin(5.0f, 1.0f) )
					[
						ErrorReporting->AsWidget()
					]
			];

		// Create comment bubble
		TSharedPtr<SCommentBubble> CommentBubble;
		const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

		SAssignNew( CommentBubble, SCommentBubble )
		.GraphNode( GraphNode )
		.Text( this, &SGraphNode::GetNodeComment )
		.OnTextCommitted( this, &SGraphNode::OnCommentTextCommitted )
		.ColorAndOpacity( CommentColor )
		.AllowPinning( true )
		.EnableTitleBarBubble( true )
		.EnableBubbleCtrls( true )
		.GraphLOD( this, &SGraphNode::GetCurrentLOD )
		.IsGraphNodeHovered( this, &SGraphNode::IsHovered );

		GetOrAddSlot( ENodeZone::TopCenter )
		.SlotOffset( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetOffset ))
		.SlotSize( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetSize ))
		.AllowScaling( TAttribute<bool>( CommentBubble.Get(), &SCommentBubble::IsScalingAllowed ))
		.VAlign( VAlign_Top )
		[
			CommentBubble.ToSharedRef()
		];

		// Create pins
		CreatePinWidgets();

		// Hide pin labels
		for (auto InputPin : this->InputPins)
		{
			if (InputPin->GetPinObj()->ParentPin == nullptr)
			{
				InputPin->SetShowLabel(false);
			}
		}

		for (auto OutputPin : this->OutputPins)
		{
			if (OutputPin->GetPinObj()->ParentPin == nullptr)
			{
				OutputPin->SetShowLabel(false);
			}
		}
	}
}

const FSlateBrush* SGraphNodeK2ArrayFunction::GetShadowBrush(bool bSelected) const
{
	UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);
	const bool bCompactMode = K2Node->ShouldDrawCompact();

	if(bCompactMode)
	{
		return bSelected ? FEditorStyle::GetBrush(TEXT("Graph.VarNode.ShadowSelected")) : FEditorStyle::GetBrush(TEXT("Graph.VarNode.Shadow"));
	}
	return SGraphNodeK2Base::GetShadowBrush(bSelected);
}