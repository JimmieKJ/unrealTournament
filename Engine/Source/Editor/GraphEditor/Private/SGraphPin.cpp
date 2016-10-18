// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetDebugUtilities.h"
#include "Editor/UnrealEd/Public/DragAndDrop/AssetDragDropOp.h"
#include "BlueprintUtilities.h"
#include "SLevelOfDetailBranchNode.h"
#include "ScopedTransaction.h"

/////////////////////////////////////////////////////
// FKnotNetCollector

struct FKnotNetCollector
{
	TSet<UEdGraphNode*> VisitedNodes;
	TSet<UEdGraphPin*> VisitedPins;

	FKnotNetCollector(UEdGraphPin* StartingPin)
	{
		TraversePin(StartingPin);
	}

	void TraversePin(UEdGraphPin* Pin)
	{
		if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Pin->GetOwningNodeUnchecked()))
		{
			TraverseNodes(Knot);
		}
		else
		{
			VisitedPins.Add(Pin);

			for (UEdGraphPin* OtherPin : Pin->LinkedTo)
			{
				UEdGraphNode* OtherNode = OtherPin->GetOwningNodeUnchecked();
				if (OtherNode && OtherNode->IsA(UK2Node_Knot::StaticClass()))
				{
					TraverseNodes(OtherNode);
				}
			}
		}
	}

	void TraverseNodes(UEdGraphNode* Node)
	{
		if (VisitedNodes.Contains(Node))
		{
			return;
		}
		VisitedNodes.Add(Node);

		for (UEdGraphPin* MyPin : Node->Pins)
		{
			VisitedPins.Add(MyPin);

			for (UEdGraphPin* OtherPin : MyPin->LinkedTo)
			{
				UEdGraphNode* OtherNode = OtherPin->GetOwningNodeUnchecked();
				if (OtherNode && OtherNode->IsA(UK2Node_Knot::StaticClass()))
				{
					TraverseNodes(OtherNode);
				}
			}
		}
	}
};

/////////////////////////////////////////////////////
// SGraphPin


SGraphPin::SGraphPin()
	: GraphPinObj(nullptr)
	, bShowLabel(true)
	, bIsMovingLinks(false)
	, PinColorModifier(FLinearColor::White)
	, CachedNodeOffset(FVector2D::ZeroVector)
{
	IsEditable = true;

	// Make these names const so they're not created for every pin

	/** Original Pin Styles */
	static const FName NAME_Pin_ConnectedHovered("Graph.Pin.ConnectedHovered");
	static const FName NAME_Pin_Connected("Graph.Pin.Connected");
	static const FName NAME_Pin_DisconnectedHovered("Graph.Pin.DisconnectedHovered");
	static const FName NAME_Pin_Disconnected("Graph.Pin.Disconnected");

	/** Variant A Pin Styles */
	static const FName NAME_Pin_ConnectedHovered_VarA("Graph.Pin.ConnectedHovered_VarA");
	static const FName NAME_Pin_Connected_VarA("Graph.Pin.Connected_VarA");
	static const FName NAME_Pin_DisconnectedHovered_VarA("Graph.Pin.DisconnectedHovered_VarA");
	static const FName NAME_Pin_Disconnected_VarA("Graph.Pin.Disconnected_VarA");

	static const FName NAME_ArrayPin_ConnectedHovered("Graph.ArrayPin.ConnectedHovered");
	static const FName NAME_ArrayPin_Connected("Graph.ArrayPin.Connected");
	static const FName NAME_ArrayPin_DisconnectedHovered("Graph.ArrayPin.DisconnectedHovered");
	static const FName NAME_ArrayPin_Disconnected("Graph.ArrayPin.Disconnected");

	static const FName NAME_RefPin_ConnectedHovered("Graph.RefPin.ConnectedHovered");
	static const FName NAME_RefPin_Connected("Graph.RefPin.Connected");
	static const FName NAME_RefPin_DisconnectedHovered("Graph.RefPin.DisconnectedHovered");
	static const FName NAME_RefPin_Disconnected("Graph.RefPin.Disconnected");

	static const FName NAME_DelegatePin_ConnectedHovered("Graph.DelegatePin.ConnectedHovered");
	static const FName NAME_DelegatePin_Connected("Graph.DelegatePin.Connected");
	static const FName NAME_DelegatePin_DisconnectedHovered("Graph.DelegatePin.DisconnectedHovered");
	static const FName NAME_DelegatePin_Disconnected("Graph.DelegatePin.Disconnected");

	static const FName NAME_Pin_Background("Graph.Pin.Background");
	static const FName NAME_Pin_BackgroundHovered("Graph.Pin.BackgroundHovered");

	const EBlueprintPinStyleType StyleType = GetDefault<UGraphEditorSettings>()->DataPinStyle;

	switch(StyleType)
	{
	case BPST_VariantA:
		CachedImg_Pin_ConnectedHovered = FEditorStyle::GetBrush( NAME_Pin_ConnectedHovered_VarA );
		CachedImg_Pin_Connected = FEditorStyle::GetBrush( NAME_Pin_Connected_VarA );
		CachedImg_Pin_DisconnectedHovered = FEditorStyle::GetBrush( NAME_Pin_DisconnectedHovered_VarA );
		CachedImg_Pin_Disconnected = FEditorStyle::GetBrush( NAME_Pin_Disconnected_VarA );
		break;
	case BPST_Original:
	default:
		CachedImg_Pin_ConnectedHovered = FEditorStyle::GetBrush( NAME_Pin_ConnectedHovered );
		CachedImg_Pin_Connected = FEditorStyle::GetBrush( NAME_Pin_Connected );
		CachedImg_Pin_DisconnectedHovered = FEditorStyle::GetBrush( NAME_Pin_DisconnectedHovered );
		CachedImg_Pin_Disconnected = FEditorStyle::GetBrush( NAME_Pin_Disconnected );
		break;
	}

	CachedImg_RefPin_ConnectedHovered = FEditorStyle::GetBrush( NAME_RefPin_ConnectedHovered );
	CachedImg_RefPin_Connected = FEditorStyle::GetBrush( NAME_RefPin_Connected );
	CachedImg_RefPin_DisconnectedHovered = FEditorStyle::GetBrush( NAME_RefPin_DisconnectedHovered );
	CachedImg_RefPin_Disconnected = FEditorStyle::GetBrush( NAME_RefPin_Disconnected );

	CachedImg_ArrayPin_ConnectedHovered = FEditorStyle::GetBrush( NAME_ArrayPin_ConnectedHovered );
	CachedImg_ArrayPin_Connected = FEditorStyle::GetBrush( NAME_ArrayPin_Connected );
	CachedImg_ArrayPin_DisconnectedHovered = FEditorStyle::GetBrush( NAME_ArrayPin_DisconnectedHovered );
	CachedImg_ArrayPin_Disconnected = FEditorStyle::GetBrush( NAME_ArrayPin_Disconnected );

	CachedImg_DelegatePin_ConnectedHovered = FEditorStyle::GetBrush( NAME_DelegatePin_ConnectedHovered );
	CachedImg_DelegatePin_Connected = FEditorStyle::GetBrush( NAME_DelegatePin_Connected );
	CachedImg_DelegatePin_DisconnectedHovered = FEditorStyle::GetBrush( NAME_DelegatePin_DisconnectedHovered );
	CachedImg_DelegatePin_Disconnected = FEditorStyle::GetBrush( NAME_DelegatePin_Disconnected );

	CachedImg_Pin_Background = FEditorStyle::GetBrush( NAME_Pin_Background );
	CachedImg_Pin_BackgroundHovered = FEditorStyle::GetBrush( NAME_Pin_BackgroundHovered );
}

void SGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	bUsePinColorForText = InArgs._UsePinColorForText;
	this->SetCursor(EMouseCursor::Default);

	Visibility = TAttribute<EVisibility>(this, &SGraphPin::GetPinVisiblity);

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	checkf(
		Schema, 
		TEXT("Missing schema for pin: %s with outer: %s of type %s"), 
		*(GraphPinObj->GetName()),
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetName()) : TEXT("NULL OUTER"), 
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetClass()->GetName()) : TEXT("NULL OUTER")
	);

	const bool bIsInput = (GetDirection() == EGPD_Input);

	// Create the pin icon widget
	TSharedRef<SWidget> ActualPinWidget =
		SAssignNew(PinImage, SImage)
		.Image(this, &SGraphPin::GetPinIcon)
		.IsEnabled(this, &SGraphPin::GetIsConnectable)
		.ColorAndOpacity(this, &SGraphPin::GetPinColor)
		.OnMouseButtonDown(this, &SGraphPin::OnPinMouseDown)
		.Cursor(this, &SGraphPin::GetPinCursor);

	// Create the pin indicator widget (used for watched values)
	static const FName NAME_NoBorder("NoBorder");
	TSharedRef<SWidget> PinStatusIndicator =
		SNew(SButton)
		.ButtonStyle(FEditorStyle::Get(), NAME_NoBorder)
		.Visibility(this, &SGraphPin::GetPinStatusIconVisibility)
		.ContentPadding(0)
		.OnClicked(this, &SGraphPin::ClickedOnPinStatusIcon)
		[
			SNew(SImage)
			.Image(this, &SGraphPin::GetPinStatusIcon)
		];

	TSharedRef<SWidget> LabelWidget = SNew(STextBlock)
		.Text(this, &SGraphPin::GetPinLabel)
		.TextStyle(FEditorStyle::Get(), InArgs._PinLabelStyle)
		.Visibility(this, &SGraphPin::GetPinLabelVisibility)
		.ColorAndOpacity(this, &SGraphPin::GetPinTextColor);

	// Create the widget used for the pin body (status indicator, label, and value)
	TSharedRef<SWrapBox> LabelAndValue =
		SNew(SWrapBox)
		.PreferredWidth(150.f);

	if (!bIsInput)
	{
		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				PinStatusIndicator
			];

		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				LabelWidget
			];
	}
	else
	{
		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				LabelWidget
			];

		TSharedRef<SWidget> ValueWidget = GetDefaultValueWidget();

		if (ValueWidget != SNullWidget::NullWidget)
		{
			LabelAndValue->AddSlot()
				.Padding(bIsInput ? FMargin(InArgs._SideToSideMargin, 0, 0, 0) : FMargin(0, 0, InArgs._SideToSideMargin, 0))
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.Padding(0.0f)
					.IsEnabled(this, &SGraphPin::IsEditingEnabled)
					[
						ValueWidget
					]
				];
		}

		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				PinStatusIndicator
			];
	}

	TSharedPtr<SHorizontalBox> PinContent;
	if (bIsInput)
	{
		// Input pin
		FullPinHorizontalRowWidget = PinContent = 
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, InArgs._SideToSideMargin, 0)
			[
				ActualPinWidget
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				LabelAndValue
			];
	}
	else
	{
		// Output pin
		FullPinHorizontalRowWidget = PinContent = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				LabelAndValue
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(InArgs._SideToSideMargin, 0, 0, 0)
			[
				ActualPinWidget
			];
	}

	// Set up a hover for pins that is tinted the color of the pin.
	SBorder::Construct(SBorder::FArguments()
		.BorderImage(this, &SGraphPin::GetPinBorder)
		.BorderBackgroundColor(this, &SGraphPin::GetPinColor)
		.OnMouseButtonDown(this, &SGraphPin::OnPinNameMouseDown)
		[
			SNew(SLevelOfDetailBranchNode)
			.UseLowDetailSlot(this, &SGraphPin::UseLowDetailPinNames)
			.LowDetail()
			[
				//@TODO: Try creating a pin-colored line replacement that doesn't measure text / call delegates but still renders
				ActualPinWidget
			]
			.HighDetail()
				[
					PinContent.ToSharedRef()
				]
		]
	);

	TAttribute<FText> ToolTipAttribute = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SGraphPin::GetTooltipText));
	SetToolTipText(ToolTipAttribute);
}

TSharedRef<SWidget>	SGraphPin::GetDefaultValueWidget()
{
	return SNew(SBox);
}


void SGraphPin::SetIsEditable(TAttribute<bool> InIsEditable)
{
	IsEditable = InIsEditable;
}


FReply SGraphPin::OnPinMouseDown( const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent )
{
	bIsMovingLinks = false;

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (!GraphPinObj->bNotConnectable && IsEditingEnabled())
		{
			if (MouseEvent.IsAltDown())
			{
				// Alt-Left clicking will break all existing connections to a pin
				const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
				Schema->BreakPinLinks(*GraphPinObj, true);
				return FReply::Handled();
			}

			auto OwnerNodePinned = OwnerNodePtr.Pin();
			if (MouseEvent.IsControlDown() && (GraphPinObj->LinkedTo.Num() > 0))
			{
				// Get a reference to the owning panel widget
				check(OwnerNodePinned.IsValid());
				TSharedPtr<SGraphPanel> OwnerPanelPtr = OwnerNodePinned->GetOwnerPanel();
				check(OwnerPanelPtr.IsValid());

				// Obtain the set of all pins within the panel
				TSet<TSharedRef<SWidget> > AllPins;
				OwnerPanelPtr->GetAllPins(AllPins);

				// Construct a UEdGraphPin->SGraphPin mapping for the full pin set
				TMap< UEdGraphPin*, TSharedRef<SGraphPin> > PinToPinWidgetMap;
				for( TSet< TSharedRef<SWidget> >::TIterator ConnectorIt(AllPins); ConnectorIt; ++ConnectorIt )
				{
					const TSharedRef<SWidget>& SomePinWidget = *ConnectorIt;
					const SGraphPin& PinWidget = static_cast<const SGraphPin&>(SomePinWidget.Get());

					PinToPinWidgetMap.Add(PinWidget.GetPinObj(), StaticCastSharedRef<SGraphPin>(SomePinWidget));
				}

				// Define a local struct to temporarily store lookup information for pins that we are currently linked to
				struct LinkedToPinInfo
				{
					// Pin name string
					FString PinName;

					// A weak reference to the node object that owns the pin
					TWeakObjectPtr<UEdGraphNode> OwnerNodePtr;
				};

				// Build a lookup table containing information about the set of pins that we're currently linked to
				TArray<LinkedToPinInfo> LinkedToPinInfoArray;
				for( TArray<UEdGraphPin*>::TIterator LinkArrayIter(GetPinObj()->LinkedTo); LinkArrayIter; ++LinkArrayIter )
				{
					if (auto PinWidget = PinToPinWidgetMap.Find(*LinkArrayIter))
					{
						check((*PinWidget)->OwnerNodePtr.IsValid());

						LinkedToPinInfo PinInfo;
						PinInfo.PinName = (*PinWidget)->GetPinObj()->PinName;
						PinInfo.OwnerNodePtr = (*PinWidget)->OwnerNodePtr.Pin()->GetNodeObj();
						LinkedToPinInfoArray.Add(PinInfo);
					}
				}

				// Control-Left clicking will break all existing connections to a pin
				// Note that for some nodes, this can cause reconstruction. In that case, pins we had previously linked to may now be destroyed.
				const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
				Schema->BreakPinLinks(*GraphPinObj, true);
				
				// Now iterate over our lookup table to find the instances of pin widgets that we had previously linked to
				TArray<TSharedRef<SGraphPin>> PinArray;
				for(auto LinkedToPinInfoIter = LinkedToPinInfoArray.CreateConstIterator(); LinkedToPinInfoIter; ++LinkedToPinInfoIter)
				{
					LinkedToPinInfo PinInfo = *LinkedToPinInfoIter;
					UEdGraphNode* OwnerNodeObj = PinInfo.OwnerNodePtr.Get();
					if(OwnerNodeObj != NULL)
					{
						for(auto PinIter = PinInfo.OwnerNodePtr.Get()->Pins.CreateConstIterator(); PinIter; ++PinIter)
						{
							UEdGraphPin* Pin = *PinIter;
							if(Pin->PinName == PinInfo.PinName)
							{
								if (auto pWidget = PinToPinWidgetMap.Find(Pin))
								{
									PinArray.Add(*pWidget);
								}
							}
						}
					}
				}

				if(PinArray.Num() > 0)
				{
					bIsMovingLinks = true;

					return FReply::Handled().BeginDragDrop(SpawnPinDragEvent(OwnerPanelPtr.ToSharedRef(), PinArray, /*bIsShiftOperation=*/ false));
				}
				else
				{
					// Shouldn't get here, but just in case we lose our previous links somehow after breaking them, we'll just skip the drag.
					return FReply::Handled();
				}
			}
			
			// Start a drag-drop on the pin
			if (ensure(OwnerNodePinned.IsValid()))
			{
				TArray<TSharedRef<SGraphPin>> PinArray;
				PinArray.Add(SharedThis(this));

				return FReply::Handled().BeginDragDrop(SpawnPinDragEvent(OwnerNodePinned->GetOwnerPanel().ToSharedRef(), PinArray, MouseEvent.IsShiftDown()));
			}
			else
			{
				return FReply::Unhandled();
			}
		}
		else
		{
			// It's not connectable, but we don't want anything above us to process this left click.
			return FReply::Handled();
		}
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SGraphPin::OnPinNameMouseDown( const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent )
{
	const float LocalX = SenderGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ).X;

	if ((GetDirection() == EGPD_Input) || FMath::Abs( SenderGeometry.GetLocalSize().X - LocalX ) < 60.f )
	{
		// Right half of the output pin or all of the input pin, treat it like a connection attempt
		return OnPinMouseDown(SenderGeometry, MouseEvent);
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SGraphPin::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bIsMovingLinks = MouseEvent.IsControlDown() && (GraphPinObj->LinkedTo.Num() > 0);

	return FReply::Unhandled();
}

TOptional<EMouseCursor::Type> SGraphPin::GetPinCursor() const
{
	check(PinImage.IsValid());

	if (PinImage->IsHovered())
	{
		if (bIsMovingLinks)
		{
			return EMouseCursor::GrabHandClosed;
		}
		else
		{
			return EMouseCursor::Crosshairs;
		}
	}
	else
	{
		return EMouseCursor::Default;
	}
}

TSharedRef<FDragDropOperation> SGraphPin::SpawnPinDragEvent(const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphPin> >& InStartingPins, bool bShiftOperation)
{
	return FDragConnection::New(InGraphPanel, InStartingPins, bShiftOperation);
}

/**
 * The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SGraphPin::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.IsShiftDown())
	{
		// Either store the shift-clicked pin or attempt to connect it if already stored
		TSharedPtr<SGraphPanel> OwnerPanelPtr = OwnerNodePtr.Pin()->GetOwnerPanel();
		check(OwnerPanelPtr.IsValid());
		if (OwnerPanelPtr->MarkedPin.IsValid())
		{
			// avoid creating transaction if toggling the marked pin
			if (!OwnerPanelPtr->MarkedPin.HasSameObject(this))
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_CreateConnection", "Create Pin Link") );
				TryHandlePinConnection(*OwnerPanelPtr->MarkedPin.Pin());
			}
			OwnerPanelPtr->MarkedPin.Reset();
		}
		else
		{
			OwnerPanelPtr->MarkedPin = SharedThis(this);
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SGraphPin::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (!bIsHovered)
	{
		FKnotNetCollector NetCollector(GetPinObj());

		TSharedPtr<SGraphPanel> Panel = OwnerNodePtr.Pin()->GetOwnerPanel();
		for (UEdGraphPin* PinInNet : NetCollector.VisitedPins)
		{
			Panel->AddPinToHoverSet(PinInNet);
			HoverPinSet.Add(PinInNet);
		}
	}

	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
}

void SGraphPin::OnMouseLeave( const FPointerEvent& MouseEvent )
{	
	TSharedPtr<SGraphPanel> Panel = OwnerNodePtr.Pin()->GetOwnerPanel();

	for (const FEdGraphPinReference& WeakPin : HoverPinSet)
	{
		if (UEdGraphPin* PinInNet = WeakPin.Get())
		{
			Panel->RemovePinFromHoverSet(PinInNet);
		}
	}
	HoverPinSet.Empty();

	SCompoundWidget::OnMouseLeave(MouseEvent);
}

void SGraphPin::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return;
	}

	// Is someone dragging a connection?
	if (Operation->IsOfType<FGraphEditorDragDropAction>())
	{
		// Ensure that the pin is valid before using it
		if(GraphPinObj != NULL && !GraphPinObj->IsPendingKill() && GraphPinObj->GetOuter() != NULL && GraphPinObj->GetOuter()->IsA(UEdGraphNode::StaticClass()))
		{
			// Inform the Drag and Drop operation that we are hovering over this pin.
			TSharedPtr<FGraphEditorDragDropAction> DragConnectionOp = StaticCastSharedPtr<FGraphEditorDragDropAction>(Operation);
			DragConnectionOp->SetHoveredPin(GraphPinObj);
		}	

		// Pins treat being dragged over the same as being hovered outside of drag and drop if they know how to respond to the drag action.
		SBorder::OnMouseEnter( MyGeometry, DragDropEvent );
	}
	else if (Operation->IsOfType<FAssetDragDropOp>())
	{
		TSharedPtr<SGraphNode> NodeWidget = OwnerNodePtr.Pin();
		if (NodeWidget.IsValid())
		{
			UEdGraphNode* Node = NodeWidget->GetNodeObj();
			if(Node != NULL && Node->GetSchema() != NULL)
			{
				TSharedPtr<FAssetDragDropOp> AssetOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
				bool bOkIcon = false;
				FString TooltipText;
				Node->GetSchema()->GetAssetsPinHoverMessage(AssetOp->AssetData, GraphPinObj, TooltipText, bOkIcon);
				const FSlateBrush* TooltipIcon = bOkIcon ? FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")) : FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));;
				AssetOp->SetToolTip(FText::FromString(TooltipText), TooltipIcon);
			}
		}
	}
}

void SGraphPin::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return;
	}

	// Is someone dragging a connection?
	if (Operation->IsOfType<FGraphEditorDragDropAction>())
	{
		// Inform the Drag and Drop operation that we are not hovering any pins
		TSharedPtr<FGraphEditorDragDropAction> DragConnectionOp = StaticCastSharedPtr<FGraphEditorDragDropAction>(Operation);
		DragConnectionOp->SetHoveredPin(nullptr);

		SBorder::OnMouseLeave(DragDropEvent);
	}

	else if (Operation->IsOfType<FAssetDragDropOp>())
	{
		TSharedPtr<FAssetDragDropOp> AssetOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
		AssetOp->ResetToDefaultToolTip();
	}
}

FReply SGraphPin::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return FReply::Unhandled();
}

bool SGraphPin::TryHandlePinConnection(SGraphPin& OtherSPin)
{
	UEdGraphPin* PinA = GetPinObj();
	UEdGraphPin* PinB = OtherSPin.GetPinObj();
	UEdGraph* MyGraphObj = PinA->GetOwningNode()->GetGraph();

	return MyGraphObj->GetSchema()->TryCreateConnection(PinA, PinB);
}

FReply SGraphPin::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<SGraphNode> NodeWidget = OwnerNodePtr.Pin();
	bool bReadOnly = NodeWidget.IsValid() ? !NodeWidget->IsNodeEditable() : false;

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid() || bReadOnly)
	{
		return FReply::Unhandled();
	}

	// Is someone dropping a connection onto this pin?
	if (Operation->IsOfType<FGraphEditorDragDropAction>())
	{
		TSharedPtr<FGraphEditorDragDropAction> DragConnectionOp = StaticCastSharedPtr<FGraphEditorDragDropAction>(Operation);

		FVector2D NodeAddPosition = FVector2D::ZeroVector;
		TSharedPtr<SGraphNode> OwnerNode = OwnerNodePtr.Pin();
		if (OwnerNode.IsValid())
		{
			NodeAddPosition	= OwnerNode->GetPosition() + MyGeometry.Position;

			//Don't have access to bounding information for node, using fixed offet that should work for most cases.
			const float FixedOffset = 200.0f;

			//Line it up vertically with pin
			NodeAddPosition.Y += MyGeometry.Size.Y;

			if(GetDirection() == EEdGraphPinDirection::EGPD_Input)
			{
				//left side just offset by fixed amount
				//@TODO: knowing the width of the node we are about to create would allow us to line this up more precisely,
				//       but this information is not available currently
				NodeAddPosition.X -= FixedOffset;
			}
			else
			{
				//right side we need the width of the pin + fixed amount because our reference position is the upper left corner of pin(which is variable length)
				NodeAddPosition.X += MyGeometry.Size.X + FixedOffset;
			}
			
		}

		return DragConnectionOp->DroppedOnPin(DragDropEvent.GetScreenSpacePosition(), NodeAddPosition);
	}
	// handle dropping an asset on the pin
	else if (Operation->IsOfType<FAssetDragDropOp>() && NodeWidget.IsValid())
	{
		UEdGraphNode* Node = NodeWidget->GetNodeObj();
		if(Node != NULL && Node->GetSchema() != NULL)
		{
			TSharedPtr<FAssetDragDropOp> AssetOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);

			Node->GetSchema()->DroppedAssetsOnPin(AssetOp->AssetData, DragDropEvent.GetScreenSpacePosition(), GraphPinObj);
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SGraphPin::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	CachedNodeOffset = AllottedGeometry.AbsolutePosition/AllottedGeometry.Scale - OwnerNodePtr.Pin()->GetUnscaledPosition();
	CachedNodeOffset.Y += AllottedGeometry.Size.Y * 0.5f;
}

UEdGraphPin* SGraphPin::GetPinObj() const
{
	return GraphPinObj;
}

/** @param OwnerNode  The SGraphNode that this pin belongs to */
void SGraphPin::SetOwner( const TSharedRef<SGraphNode> OwnerNode )
{
	check( !OwnerNodePtr.IsValid() );
	OwnerNodePtr = OwnerNode;
}

EVisibility SGraphPin::IsPinVisibleAsAdvanced() const
{
	bool bHideAdvancedPin = false;
	const TSharedPtr<SGraphNode> NodeWidget = OwnerNodePtr.Pin();
	if (NodeWidget.IsValid())
	{
		if(const UEdGraphNode* Node = NodeWidget->GetNodeObj())
		{
			bHideAdvancedPin = (ENodeAdvancedPins::Hidden == Node->AdvancedPinDisplay);
		}
	}

	const bool bIsAdvancedPin = GraphPinObj && !GraphPinObj->IsPendingKill() && GraphPinObj->bAdvancedView;
	const bool bCanBeHidden = !IsConnected();
	return (bIsAdvancedPin && bHideAdvancedPin && bCanBeHidden) ? EVisibility::Collapsed : EVisibility::Visible;
}

FVector2D SGraphPin::GetNodeOffset() const
{
	return CachedNodeOffset;
}

FText SGraphPin::GetPinLabel() const
{
	if (UEdGraphNode* GraphNode = GetPinObj()->GetOwningNodeUnchecked())
	{
		return GraphNode->GetPinDisplayName(GetPinObj());
	}
	return FText::GetEmpty();
}

/** @return whether this pin is incoming or outgoing */
EEdGraphPinDirection SGraphPin::GetDirection() const
{
	return static_cast<EEdGraphPinDirection>(GraphPinObj->Direction);
}

/** @return whether this pin is an array value */
bool SGraphPin::IsArray() const
{
	return GraphPinObj->PinType.bIsArray;
}

bool SGraphPin::IsByRef() const
{
	return GraphPinObj->PinType.bIsReference;
}

bool SGraphPin::IsByMutableRef() const
{
	return GraphPinObj->PinType.bIsReference && !GraphPinObj->PinType.bIsConst;
}

bool SGraphPin::IsDelegate() const
{
	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	return Schema && Schema->IsDelegateCategory(GraphPinObj->PinType.PinCategory);
}

bool SGraphPin::IsByConstRef() const
{
	return GraphPinObj->PinType.bIsReference && GraphPinObj->PinType.bIsConst;
}

/** @return whether this pin is connected to another pin */
bool SGraphPin::IsConnected() const
{
	return GraphPinObj->LinkedTo.Num() > 0;
}

/** @return The brush with which to pain this graph pin's incoming/outgoing bullet point */
const FSlateBrush* SGraphPin::GetPinIcon() const
{
	if (IsArray())
	{
		if (IsConnected())
		{
			return IsHovered() ? CachedImg_ArrayPin_ConnectedHovered : CachedImg_ArrayPin_Connected;
		}
		else
		{
			return IsHovered() ? CachedImg_ArrayPin_DisconnectedHovered : CachedImg_ArrayPin_Disconnected;
		}
	}
	else if(IsDelegate())
	{
		if (IsConnected())
		{
			return IsHovered() ? CachedImg_DelegatePin_ConnectedHovered : CachedImg_DelegatePin_Connected;		
		}
		else
		{
			return IsHovered() ? CachedImg_DelegatePin_DisconnectedHovered : CachedImg_DelegatePin_Disconnected;
		}
	}
	else if (GraphPinObj->bDisplayAsMutableRef || IsByMutableRef())
	{
		if (IsConnected())
		{
			return IsHovered() ? CachedImg_RefPin_ConnectedHovered : CachedImg_RefPin_Connected;		
		}
		else
		{
			return IsHovered() ? CachedImg_RefPin_DisconnectedHovered : CachedImg_RefPin_Disconnected;
		}

	}
	else
	{
		if (IsConnected())
		{
			return IsHovered() ? CachedImg_Pin_ConnectedHovered : CachedImg_Pin_Connected;
		}
		else
		{
			return IsHovered() ? CachedImg_Pin_DisconnectedHovered : CachedImg_Pin_Disconnected;
		}
	}
}

const FSlateBrush* SGraphPin::GetPinBorder() const
{
	bool bIsMarkedPin = false;
	TSharedPtr<SGraphPanel> OwnerPanelPtr = OwnerNodePtr.Pin()->GetOwnerPanel();
	check(OwnerPanelPtr.IsValid());
	if (OwnerPanelPtr->MarkedPin.IsValid())
	{
		bIsMarkedPin = (OwnerPanelPtr->MarkedPin.Pin() == SharedThis(this));
	}

	return (IsHovered() || bIsMarkedPin || GraphPinObj->bIsDiffing) ? CachedImg_Pin_BackgroundHovered : CachedImg_Pin_Background;
}


FSlateColor SGraphPin::GetPinColor() const
{
	if(GraphPinObj->bIsDiffing)
	{
		return FSlateColor(FLinearColor(0.9f,0.2f,0.15f));
	}
	if (const UEdGraphSchema* Schema = GraphPinObj->GetSchema())
	{
		if(!GetPinObj()->GetOwningNode()->IsNodeEnabled() || !IsEditingEnabled())
		{
			return Schema->GetPinTypeColor(GraphPinObj->PinType) * FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);
		}

		return Schema->GetPinTypeColor(GraphPinObj->PinType) * PinColorModifier;
	}

	return FLinearColor::White;
}

FSlateColor SGraphPin::GetPinTextColor() const
{
	// If there is no schema there is no owning node (or basically this is a deleted node)
	if (UEdGraphNode* GraphNode = GraphPinObj->GetOwningNodeUnchecked())
	{
		if(!GraphNode->IsNodeEnabled() || !IsEditingEnabled())
		{
			return FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);
		}
		else if(bUsePinColorForText)
		{
			return GetPinColor();
		}
	}
	return FLinearColor::White;
}


const FSlateBrush* SGraphPin::GetPinStatusIcon() const
{
	UEdGraphPin* WatchedPin = ((GraphPinObj->Direction == EGPD_Input) && (GraphPinObj->LinkedTo.Num() > 0)) ? GraphPinObj->LinkedTo[0] : GraphPinObj;

	if (UEdGraphNode* GraphNode = WatchedPin->GetOwningNodeUnchecked())
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(GraphNode);

		if (FKismetDebugUtilities::IsPinBeingWatched(Blueprint, WatchedPin) )
		{
			return FEditorStyle::GetBrush( TEXT("Graph.WatchedPinIcon_Pinned") );
		}
	}

	return nullptr;
}

EVisibility SGraphPin::GetPinStatusIconVisibility() const
{
	UEdGraphPin const* WatchedPin = ((GraphPinObj->Direction == EGPD_Input) && (GraphPinObj->LinkedTo.Num() > 0)) ? GraphPinObj->LinkedTo[0] : GraphPinObj;

	UEdGraphSchema const* Schema = GraphPinObj->GetSchema();
	return Schema && Schema->IsPinBeingWatched(WatchedPin) ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SGraphPin::ClickedOnPinStatusIcon()
{
	UEdGraphPin* WatchedPin = ((GraphPinObj->Direction == EGPD_Input) && (GraphPinObj->LinkedTo.Num() > 0)) ? GraphPinObj->LinkedTo[0] : GraphPinObj;

	UEdGraphSchema const* Schema = GraphPinObj->GetSchema();
	Schema->ClearPinWatch(WatchedPin);

	return FReply::Handled();
}

EVisibility SGraphPin::GetDefaultValueVisibility() const
{
	// First ask schema
	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	if (Schema == nullptr || Schema->ShouldHidePinDefaultValue(GraphPinObj))
	{
		return EVisibility::Collapsed;
	}

	if (GraphPinObj->bNotConnectable)
	{
		// The only reason this pin exists is to show something, so do so
		return EVisibility::Visible;
	}

	if (GraphPinObj->Direction == EGPD_Output)
	{
		//@TODO: Should probably be a bLiteralOutput flag or a Schema call
		return EVisibility::Collapsed;
	}
	else
	{
		return IsConnected() ? EVisibility::Collapsed : EVisibility::Visible;
	}
}

void SGraphPin::SetShowLabel(bool bNewShowLabel)
{
	bShowLabel = bNewShowLabel;
}

FText SGraphPin::GetTooltipText() const
{
	FText HoverText = FText::GetEmpty();

	UEdGraphNode* GraphNode = GraphPinObj && !GraphPinObj->IsPendingKill() ? GraphPinObj->GetOwningNodeUnchecked() : nullptr;
	if (GraphNode != nullptr)
	{
		FString HoverStr;
		GraphNode->GetPinHoverText(*GraphPinObj, /*out*/HoverStr);
		if (!HoverStr.IsEmpty())
		{
			HoverText = FText::FromString(HoverStr);
		}
	}

	return HoverText;
}

bool SGraphPin::IsEditingEnabled() const
{
	if (OwnerNodePtr.IsValid())
	{
		return OwnerNodePtr.Pin()->IsNodeEditable() && IsEditable.Get();
	}
	return IsEditable.Get();
}

bool SGraphPin::UseLowDetailPinNames() const
{
	if (SGraphNode* MyOwnerNode = OwnerNodePtr.Pin().Get())
	{
		return MyOwnerNode->GetOwnerPanel()->GetCurrentLOD() <= EGraphRenderingLOD::LowDetail;
	}
	else
	{
		return false;
	}
}

EVisibility SGraphPin::GetPinVisiblity() const
{
	// The pin becomes too small to use at low LOD, so disable the hit test.
	if(UseLowDetailPinNames())
	{
		return EVisibility::HitTestInvisible;
	}
	return EVisibility::Visible;
}

bool SGraphPin::GetIsConnectable() const
{
	const bool bCanConnectToPin = !GraphPinObj->bNotConnectable;
	return bCanConnectToPin;
}