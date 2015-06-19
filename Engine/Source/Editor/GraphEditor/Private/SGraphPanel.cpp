// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GraphEditorCommon.h"
#include "NodeFactory.h"

#include "Editor/UnrealEd/Public/DragAndDrop/ActorDragDropGraphEdOp.h"
#include "Editor/UnrealEd/Public/DragAndDrop/AssetDragDropOp.h"
#include "Editor/UnrealEd/Public/DragAndDrop/LevelDragDropOp.h"

#include "GraphEditorActions.h"
#include "UICommandInfo.h"
#include "InputChord.h"

#include "ConnectionDrawingPolicy.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "AnimGraphConnectionDrawingPolicy.h"
#include "SoundCueGraphConnectionDrawingPolicy.h"
#include "MaterialGraphConnectionDrawingPolicy.h"
#include "StateMachineConnectionDrawingPolicy.h"

#include "AssetSelection.h"
#include "ComponentAssetBroker.h"

#include "KismetNodes/KismetNodeInfoContext.h"
#include "GraphDiffControl.h"

#include "AnimationGraphSchema.h"
#include "AnimationStateMachineSchema.h"

DEFINE_LOG_CATEGORY_STATIC(LogGraphPanel, Log, All);

//////////////////////////////////////////////////////////////////////////
// FGraphPinHandle

FGraphPinHandle::FGraphPinHandle(UEdGraphPin* InPin)
{
	if (InPin != nullptr)
	{
		if (UEdGraphNode* Node = InPin->GetOwningNodeUnchecked())
		{
			PinName = InPin->PinName;
			NodeGuid = Node->NodeGuid;
		}
	}
}

TSharedPtr<SGraphPin> FGraphPinHandle::FindInGraphPanel(const SGraphPanel& InPanel) const
{
	// First off, find the node
	if (NodeGuid.IsValid())
	{
		TSharedPtr<SGraphNode> GraphNode = InPanel.GetNodeWidgetFromGuid(NodeGuid);
		if (GraphNode.IsValid())
		{
			UEdGraphNode* Node = GraphNode->GetNodeObj();

			if (UEdGraphPin* Pin = Node->FindPin(PinName))
			{
				return GraphNode->FindWidgetForPin(Pin);
			}
		}
	}

	return TSharedPtr<SGraphPin>();
}

//////////////////////////////////////////////////////////////////////////
// SGraphPanel

void SGraphPanel::Construct( const SGraphPanel::FArguments& InArgs )
{
	SNodePanel::Construct();

	this->OnGetContextMenuFor = InArgs._OnGetContextMenuFor;
	this->GraphObj = InArgs._GraphObj;
	this->GraphObjToDiff = InArgs._GraphObjToDiff;
	this->SelectionManager.OnSelectionChanged = InArgs._OnSelectionChanged;
	this->IsEditable = InArgs._IsEditable;
	this->OnNodeDoubleClicked = InArgs._OnNodeDoubleClicked;
	this->OnDropActor = InArgs._OnDropActor;
	this->OnDropStreamingLevel = InArgs._OnDropStreamingLevel;
	this->OnVerifyTextCommit = InArgs._OnVerifyTextCommit;
	this->OnTextCommitted = InArgs._OnTextCommitted;
	this->OnSpawnNodeByShortcut = InArgs._OnSpawnNodeByShortcut;
	this->OnUpdateGraphPanel = InArgs._OnUpdateGraphPanel;
	this->OnDisallowedPinConnection = InArgs._OnDisallowedPinConnection;

	this->bPreservePinPreviewConnection = false;
	this->PinVisibility = SGraphEditor::Pin_Show;

	CachedAllottedGeometryScaledSize = FVector2D(160, 120);
	if (InArgs._InitialZoomToFit)
	{
		ZoomToFit(/*bOnlySelection=*/ false);
		bTeleportInsteadOfScrollingWhenZoomingToFit = true;
	}

	BounceCurve.AddCurve(0.0f, 1.0f);

	FEditorDelegates::BeginPIE.AddRaw( this, &SGraphPanel::OnBeginPIE );
	FEditorDelegates::EndPIE.AddRaw( this, &SGraphPanel::OnEndPIE );

	// Register for notifications
	MyRegisteredGraphChangedDelegate = FOnGraphChanged::FDelegate::CreateSP(this, &SGraphPanel::OnGraphChanged);
	MyRegisteredGraphChangedDelegateHandle = this->GraphObj->AddOnGraphChangedHandler(MyRegisteredGraphChangedDelegate);
	
	ShowGraphStateOverlay = InArgs._ShowGraphStateOverlay;

	SavedMousePosForOnPaintEventLocalSpace = FVector2D::ZeroVector;
	PreviousFrameSavedMousePosForSplineOverlap = FVector2D::ZeroVector;
}

SGraphPanel::~SGraphPanel()
{
	FEditorDelegates::BeginPIE.RemoveAll( this );
	FEditorDelegates::EndPIE.RemoveAll( this );

	this->GraphObj->RemoveOnGraphChangedHandler(MyRegisteredGraphChangedDelegateHandle);
}

//////////////////////////////////////////////////////////////////////////

int32 SGraphPanel::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	CachedAllottedGeometryScaledSize = AllottedGeometry.Size * AllottedGeometry.Scale;

	//Style used for objects that are the same between revisions
	FWidgetStyle FadedStyle = InWidgetStyle;
	FadedStyle.BlendColorAndOpacityTint(FLinearColor(0.45f,0.45f,0.45f,0.45f));

	// First paint the background
	const UEditorExperimentalSettings& Options = *GetDefault<UEditorExperimentalSettings>();

	const FSlateBrush* BackgroundImage = FEditorStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
	PaintBackgroundAsLines(BackgroundImage, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId);

	const float ZoomFactor = AllottedGeometry.Scale * GetZoomAmount();

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildNodes(AllottedGeometry, ArrangedChildren);

	// Determine some 'global' settings based on current LOD
	const bool bDrawShadowsThisFrame = GetCurrentLOD() > EGraphRenderingLOD::LowestDetail;

	// Because we paint multiple children, we must track the maximum layer id that they produced in case one of our parents
	// wants to an overlay for all of its contents.

	// Save LayerId for comment boxes to ensure they always appear below nodes & wires
	const int32 CommentNodeShadowLayerId = LayerId++;
	const int32 CommentNodeLayerId = LayerId++;

	// Save a LayerId for wires, which appear below nodes but above comments
	// We will draw them later, along with the arrows which appear above nodes.
	const int32 WireLayerId = LayerId++;

	const int32 NodeShadowsLayerId = LayerId;
	const int32 NodeLayerId = NodeShadowsLayerId + 1;
	int32 MaxLayerId = NodeLayerId;

	const FVector2D NodeShadowSize = GetDefault<UGraphEditorSettings>()->GetShadowDeltaSize();
	const UEdGraphSchema* Schema = GraphObj->GetSchema();

	// Draw the child nodes
	{
		// When drawing a marquee, need a preview of what the selection will be.
		const FGraphPanelSelectionSet* SelectionToVisualize = &(SelectionManager.SelectedNodes);
		FGraphPanelSelectionSet SelectionPreview;
		if ( Marquee.IsValid() )
		{			
			ApplyMarqueeSelection(Marquee, SelectionManager.SelectedNodes, SelectionPreview);
			SelectionToVisualize = &SelectionPreview;
		}

		// Context for rendering node infos
		FKismetNodeInfoContext Context(GraphObj);

		TArray<FGraphDiffControl::FNodeMatch> NodeMatches;
		for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
		{
			FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
			TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);
			
			// Examine node to see what layers we should be drawing in
			int32 ShadowLayerId = NodeShadowsLayerId;
			int32 ChildLayerId = NodeLayerId;

			// If a comment node, draw in the dedicated comment slots
			{
				UObject* NodeObj = ChildNode->GetObjectBeingDisplayed();
				if (NodeObj && NodeObj->IsA(UEdGraphNode_Comment::StaticClass()))
				{
					ShadowLayerId = CommentNodeShadowLayerId;
					ChildLayerId = CommentNodeLayerId;
				}
			}


			const bool bNodeIsVisible = FSlateRect::DoRectanglesIntersect( CurWidget.Geometry.GetClippingRect(), MyClippingRect );

			if (bNodeIsVisible)
			{
				const bool bSelected = SelectionToVisualize->Contains( StaticCastSharedRef<SNodePanel::SNode>(CurWidget.Widget)->GetObjectBeingDisplayed() );

				// Handle Node renaming once the node is visible
				if( bSelected && ChildNode->IsRenamePending() )
				{
					// Only open a rename when the window has focus
					TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
					if (!OwnerWindow.IsValid() || FSlateApplication::Get().HasFocusedDescendants(OwnerWindow.ToSharedRef()))
					{
						ChildNode->ApplyRename();
					}
				}

				// Draw the node's shadow.
				if (bDrawShadowsThisFrame || bSelected)
				{
					const FSlateBrush* ShadowBrush = ChildNode->GetShadowBrush(bSelected);
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						ShadowLayerId,
						CurWidget.Geometry.ToInflatedPaintGeometry(NodeShadowSize),
						ShadowBrush,
						MyClippingRect
						);
				}

				// Draw the comments and information popups for this node, if it has any.
				{
					const SNodePanel::SNode::FNodeSlot& CommentSlot = ChildNode->GetOrAddSlot( ENodeZone::TopCenter );
					float CommentBubbleY = -CommentSlot.Offset.Get().Y;
					Context.bSelected = bSelected;
					TArray<FGraphInformationPopupInfo> Popups;

					{
						ChildNode->GetNodeInfoPopups(&Context, /*out*/ Popups);
					}

					for (int32 PopupIndex = 0; PopupIndex < Popups.Num(); ++PopupIndex)
					{
						FGraphInformationPopupInfo& Popup = Popups[PopupIndex];
						PaintComment(Popup.Message, CurWidget.Geometry, MyClippingRect, OutDrawElements, ChildLayerId, Popup.BackgroundColor, /*inout*/ CommentBubbleY, InWidgetStyle);
					}
				}

				int32 CurWidgetsMaxLayerId;
				{
					UEdGraphNode* NodeObj = Cast<UEdGraphNode>(ChildNode->GetObjectBeingDisplayed());

					/** When diffing nodes, nodes that are different between revisions are opaque, nodes that have not changed are faded */
					FGraphDiffControl::FNodeMatch NodeMatch = FGraphDiffControl::FindNodeMatch(GraphObjToDiff, NodeObj, NodeMatches);
					if (NodeMatch.IsValid())
					{
						NodeMatches.Add(NodeMatch);
					}
					const bool bNodeIsDifferent = (!GraphObjToDiff || NodeMatch.Diff());

					/* When dragging off a pin, we want to duck the alpha of some nodes */
					TSharedPtr< SGraphPin > OnlyStartPin = (1 == PreviewConnectorFromPins.Num()) ? PreviewConnectorFromPins[0].FindInGraphPanel(*this) : TSharedPtr< SGraphPin >();
					const bool bNodeIsNotUsableInCurrentContext = Schema->FadeNodeWhenDraggingOffPin(NodeObj, OnlyStartPin.IsValid() ? OnlyStartPin.Get()->GetPinObj() : nullptr);
					const FWidgetStyle& NodeStyleToUse = (bNodeIsDifferent && !bNodeIsNotUsableInCurrentContext)? InWidgetStyle : FadedStyle;

					// Draw the node.O
					CurWidgetsMaxLayerId = CurWidget.Widget->Paint( Args.WithNewParent(this), CurWidget.Geometry, MyClippingRect, OutDrawElements, ChildLayerId, NodeStyleToUse, ShouldBeEnabled( bParentEnabled ) );
				}

				// Draw the node's overlay, if it has one.
				{
					// Get its size
					const FVector2D WidgetSize = CurWidget.Geometry.Size;

					{
						TArray<FOverlayBrushInfo> OverlayBrushes;
						ChildNode->GetOverlayBrushes(bSelected, WidgetSize, /*out*/ OverlayBrushes);

						for (int32 BrushIndex = 0; BrushIndex < OverlayBrushes.Num(); ++BrushIndex)
						{
							FOverlayBrushInfo& OverlayInfo = OverlayBrushes[BrushIndex];
							const FSlateBrush* OverlayBrush = OverlayInfo.Brush;
							if (OverlayBrush != nullptr)
							{
								FPaintGeometry BouncedGeometry = CurWidget.Geometry.ToPaintGeometry(OverlayInfo.OverlayOffset, OverlayBrush->ImageSize, 1.f);

								// Handle bouncing during PIE
								const float BounceValue = FMath::Sin(2.0f * PI * BounceCurve.GetLerp());
								BouncedGeometry.DrawPosition += (OverlayInfo.AnimationEnvelope * BounceValue * ZoomFactor);

								CurWidgetsMaxLayerId++;
								FSlateDrawElement::MakeBox(
									OutDrawElements,
									CurWidgetsMaxLayerId,
									BouncedGeometry,
									OverlayBrush,
									MyClippingRect
									);
							}

						}
					}

					{
						TArray<FOverlayWidgetInfo> OverlayWidgets = ChildNode->GetOverlayWidgets(bSelected, WidgetSize);

						for (int32 WidgetIndex = 0; WidgetIndex < OverlayWidgets.Num(); ++WidgetIndex)
						{
							FOverlayWidgetInfo& OverlayInfo = OverlayWidgets[WidgetIndex];
							if(OverlayInfo.Widget->GetVisibility() == EVisibility::Visible)
							{
								// call SlatePrepass as these widgets are not in the 'normal' child hierarchy
								OverlayInfo.Widget->SlatePrepass(AllottedGeometry.GetAccumulatedLayoutTransform().GetScale());

								const FGeometry WidgetGeometry = CurWidget.Geometry.MakeChild(OverlayInfo.OverlayOffset, OverlayInfo.Widget->GetDesiredSize());

								OverlayInfo.Widget->Paint(Args.WithNewParent(this), WidgetGeometry, MyClippingRect, OutDrawElements, CurWidgetsMaxLayerId, InWidgetStyle, bParentEnabled);
							}
						}
					}
				}

				MaxLayerId = FMath::Max( MaxLayerId, CurWidgetsMaxLayerId + 1 );
			}
		}
	}

	MaxLayerId += 1;


	// Draw connections between pins 
	if (Children.Num() > 0 )
	{

		//@TODO: Pull this into a factory like the pin and node ones
		FConnectionDrawingPolicy* ConnectionDrawingPolicy;
		{
			ConnectionDrawingPolicy = Schema->CreateConnectionDrawingPolicy(WireLayerId, MaxLayerId, ZoomFactor, MyClippingRect, OutDrawElements, GraphObj);
			if (!ConnectionDrawingPolicy)
			{
				if (Schema->IsA(UAnimationGraphSchema::StaticClass()))
				{
					ConnectionDrawingPolicy = new FAnimGraphConnectionDrawingPolicy(WireLayerId, MaxLayerId, ZoomFactor, MyClippingRect, OutDrawElements, GraphObj);
				}
				else if (Schema->IsA(UAnimationStateMachineSchema::StaticClass()))
				{
					ConnectionDrawingPolicy = new FStateMachineConnectionDrawingPolicy(WireLayerId, MaxLayerId, ZoomFactor, MyClippingRect, OutDrawElements, GraphObj);
				}
				else if (Schema->IsA(UEdGraphSchema_K2::StaticClass()))
				{
					ConnectionDrawingPolicy = new FKismetConnectionDrawingPolicy(WireLayerId, MaxLayerId, ZoomFactor, MyClippingRect, OutDrawElements, GraphObj);
				}
				else if (Schema->IsA(USoundCueGraphSchema::StaticClass()))
				{
					ConnectionDrawingPolicy = new FSoundCueGraphConnectionDrawingPolicy(WireLayerId, MaxLayerId, ZoomFactor, MyClippingRect, OutDrawElements, GraphObj);
				}
				else if (Schema->IsA(UMaterialGraphSchema::StaticClass()))
				{
					ConnectionDrawingPolicy = new FMaterialGraphConnectionDrawingPolicy(WireLayerId, MaxLayerId, ZoomFactor, MyClippingRect, OutDrawElements, GraphObj);
				}
				else
				{
					ConnectionDrawingPolicy = new FConnectionDrawingPolicy(WireLayerId, MaxLayerId, ZoomFactor, MyClippingRect, OutDrawElements);
				}
			}
		}

		TArray<TSharedPtr<SGraphPin>> OverridePins;
		for (const FGraphPinHandle& Handle : PreviewConnectorFromPins)
		{
			TSharedPtr<SGraphPin> Pin = Handle.FindInGraphPanel(*this);
			if (Pin.IsValid())
			{
				OverridePins.Add(Pin);
			}
		}
		ConnectionDrawingPolicy->SetHoveredPins(CurrentHoveredPins, OverridePins, TimeWhenMouseEnteredPin);
		ConnectionDrawingPolicy->SetMarkedPin(MarkedPin);
		ConnectionDrawingPolicy->SetMousePosition(AllottedGeometry.AbsolutePosition + SavedMousePosForOnPaintEventLocalSpace);

		// Get the set of pins for all children and synthesize geometry for culled out pins so lines can be drawn to them.
		TMap<TSharedRef<SWidget>, FArrangedWidget> PinGeometries;
		TSet< TSharedRef<SWidget> > VisiblePins;
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(Children[ChildIndex]);

			// If this is a culled node, approximate the pin geometry to the corner of the node it is within
			if (IsNodeCulled(ChildNode, AllottedGeometry))
			{
				TArray< TSharedRef<SWidget> > NodePins;
				ChildNode->GetPins(NodePins);

				const FVector2D NodeLoc = ChildNode->GetPosition();
				const FGeometry SynthesizedNodeGeometry(GraphCoordToPanelCoord(NodeLoc), AllottedGeometry.AbsolutePosition, FVector2D::ZeroVector, 1.f);

				for (TArray< TSharedRef<SWidget> >::TConstIterator NodePinIterator(NodePins); NodePinIterator; ++NodePinIterator)
				{
					const SGraphPin& PinWidget = static_cast<const SGraphPin&>((*NodePinIterator).Get());
					FVector2D PinLoc = NodeLoc + PinWidget.GetNodeOffset();

					const FGeometry SynthesizedPinGeometry(GraphCoordToPanelCoord(PinLoc), AllottedGeometry.AbsolutePosition, FVector2D::ZeroVector, 1.f);
					PinGeometries.Add(*NodePinIterator, FArrangedWidget(*NodePinIterator, SynthesizedPinGeometry));
				}

				// Also add synthesized geometries for culled nodes
				ArrangedChildren.AddWidget( FArrangedWidget(ChildNode, SynthesizedNodeGeometry) );
			}
			else
			{
				ChildNode->GetPins(VisiblePins);
			}
		}

		// Now get the pin geometry for all visible children and append it to the PinGeometries map
		TMap<TSharedRef<SWidget>, FArrangedWidget> VisiblePinGeometries;
		{
			this->FindChildGeometries(AllottedGeometry, VisiblePins, VisiblePinGeometries);
			PinGeometries.Append(VisiblePinGeometries);
		}

		// Draw preview connections (only connected on one end)
		if (PreviewConnectorFromPins.Num() > 0)
		{
			for (const FGraphPinHandle& Handle : PreviewConnectorFromPins)
			{
				TSharedPtr< SGraphPin > CurrentStartPin = Handle.FindInGraphPanel(*this);
				if (!CurrentStartPin.IsValid())
				{
					continue;
				}
				const FArrangedWidget* PinGeometry = PinGeometries.Find( CurrentStartPin.ToSharedRef() );

				if (PinGeometry != nullptr)
				{
					FVector2D StartPoint;
					FVector2D EndPoint;

					if (CurrentStartPin->GetDirection() == EGPD_Input)
					{
						StartPoint = AllottedGeometry.AbsolutePosition + PreviewConnectorEndpoint;
						EndPoint = FGeometryHelper::VerticalMiddleLeftOf( PinGeometry->Geometry ) - FVector2D(ConnectionDrawingPolicy->ArrowRadius.X, 0);
					}
					else
					{
						StartPoint = FGeometryHelper::VerticalMiddleRightOf( PinGeometry->Geometry );
						EndPoint = AllottedGeometry.AbsolutePosition + PreviewConnectorEndpoint;
					}

					ConnectionDrawingPolicy->DrawPreviewConnector(PinGeometry->Geometry, StartPoint, EndPoint, CurrentStartPin.Get()->GetPinObj());
				}

				//@TODO: Re-evaluate this incompatible mojo; it's mutating every pin state every frame to accomplish a visual effect
				ConnectionDrawingPolicy->SetIncompatiblePinDrawState(CurrentStartPin, VisiblePins);
			}
		}
		else
		{
			//@TODO: Re-evaluate this incompatible mojo; it's mutating every pin state every frame to accomplish a visual effect
			ConnectionDrawingPolicy->ResetIncompatiblePinDrawState(VisiblePins);
		}

		// Draw all regular connections
		ConnectionDrawingPolicy->Draw(PinGeometries, ArrangedChildren);

		// Pull back data from the drawing policy
		{
			FGraphSplineOverlapResult OverlapData = ConnectionDrawingPolicy->SplineOverlapResult;

			if (OverlapData.IsValid())
			{
				OverlapData.ComputeBestPin();

				// Only allow spline overlaps when there is no node under the cursor (unless it is a comment box)
				const FVector2D PaintAbsoluteSpaceMousePos = AllottedGeometry.AbsolutePosition + SavedMousePosForOnPaintEventLocalSpace;
				const int32 HoveredNodeIndex = SWidget::FindChildUnderPosition(ArrangedChildren, PaintAbsoluteSpaceMousePos);
				if (HoveredNodeIndex != INDEX_NONE)
				{
					TSharedRef<SGraphNode> HoveredNode = StaticCastSharedRef<SGraphNode>(ArrangedChildren[HoveredNodeIndex].Widget);
					UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(HoveredNode->GetObjectBeingDisplayed());
					if (CommentNode == nullptr)
					{
						// Wasn't a comment node, disallow the spline interaction
						OverlapData = FGraphSplineOverlapResult();
					}
				}
			}

			// Update the spline hover state
			const_cast<SGraphPanel*>(this)->OnSplineHoverStateChanged(OverlapData);
		}

		delete ConnectionDrawingPolicy;
	}

	// Draw a shadow overlay around the edges of the graph
	++MaxLayerId;
	PaintSurroundSunkenShadow(FEditorStyle::GetBrush(TEXT("Graph.Shadow")), AllottedGeometry, MyClippingRect, OutDrawElements, MaxLayerId);

	if (ShowGraphStateOverlay.Get())
	{
		const FSlateBrush* BorderBrush = nullptr;
		if ((GEditor->bIsSimulatingInEditor || GEditor->PlayWorld != nullptr))
		{
			// Draw a surrounding indicator when PIE is active, to make it clear that the graph is read-only, etc...
			BorderBrush = FEditorStyle::GetBrush(TEXT("Graph.PlayInEditor"));
		}
		else if (!IsEditable.Get())
		{
			// Draw a different border when we're not simulating but the graph is read-only
			BorderBrush = FEditorStyle::GetBrush(TEXT("Graph.ReadOnlyBorder"));
		}

		if (BorderBrush != nullptr)
		{
			// Actually draw the border
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				MaxLayerId,
				AllottedGeometry.ToPaintGeometry(),
				BorderBrush,
				MyClippingRect
				);
		}
	}

	// Draw the marquee selection rectangle
	PaintMarquee(AllottedGeometry, MyClippingRect, OutDrawElements, MaxLayerId);

	// Draw the software cursor
	++MaxLayerId;
	PaintSoftwareCursor(AllottedGeometry, MyClippingRect, OutDrawElements, MaxLayerId);

	return MaxLayerId;
}

void SGraphPanel::OnSplineHoverStateChanged(const FGraphSplineOverlapResult& NewSplineHoverState)
{
	TSharedPtr<SGraphPin> OldPinWidget = PreviousFrameSplineOverlap.GetBestPinWidget(*this);
	PreviousFrameSplineOverlap = NewSplineHoverState;
	TSharedPtr<SGraphPin> NewPinWidget = PreviousFrameSplineOverlap.GetBestPinWidget(*this);

	PreviousFrameSavedMousePosForSplineOverlap = SavedMousePosForOnPaintEventLocalSpace;

	// Handle mouse enter/leaves on the associated pin
	if (OldPinWidget != NewPinWidget)
	{
		if (OldPinWidget.IsValid())
		{
			OldPinWidget->OnMouseLeave(LastPointerEvent);
		}

		if (NewPinWidget.IsValid())
		{
			NewPinWidget->OnMouseEnter(LastPointerGeometry, LastPointerEvent);

			// Get the pin/wire glowing quicker, since it's a direct selection (this time was already set to 'now' as part of entering the pin)
			//@TODO: Source this parameter from the graph rendering settings once it is there (see code in ApplyHoverDeemphasis)
			TimeWhenMouseEnteredPin -= 0.75f;
		}
	}
}

bool SGraphPanel::SupportsKeyboardFocus() const
{
	return true;
}

void SGraphPanel::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	SNodePanel::OnArrangeChildren(AllottedGeometry, ArrangedChildren);

	FArrangedChildren MyArrangedChildren(ArrangedChildren.GetFilter());
	for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
		TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);

		TArray<FOverlayWidgetInfo> OverlayWidgets = ChildNode->GetOverlayWidgets(false, CurWidget.Geometry.Size);

		for (int32 WidgetIndex = 0; WidgetIndex < OverlayWidgets.Num(); ++WidgetIndex)
		{
			FOverlayWidgetInfo& OverlayInfo = OverlayWidgets[WidgetIndex];

			MyArrangedChildren.AddWidget(AllottedGeometry.MakeChild( OverlayInfo.Widget.ToSharedRef(), CurWidget.Geometry.Position + OverlayInfo.OverlayOffset, OverlayInfo.Widget->GetDesiredSize(), GetZoomAmount() ));
		}
	}

	ArrangedChildren.Append(MyArrangedChildren);
}

TSharedPtr<IToolTip> SGraphPanel::GetToolTip()
{
	if (SGraphPin* BestPinFromHoveredSpline = GetBestPinFromHoveredSpline())
	{
		return BestPinFromHoveredSpline->GetToolTip();
	}

	return SNodePanel::GetToolTip();
}

void SGraphPanel::UpdateSelectedNodesPositions(FVector2D PositionIncrement)
{
	for (FGraphPanelSelectionSet::TIterator NodeIt(SelectionManager.SelectedNodes); NodeIt; ++NodeIt)
	{
		TSharedRef<SNode>* pWidget = NodeToWidgetLookup.Find(*NodeIt);
		if (pWidget != nullptr)
		{
			SNode& Widget = pWidget->Get();
			SNode::FNodeSet NodeFilter;
			Widget.MoveTo(Widget.GetPosition() + PositionIncrement, NodeFilter);
		}
	}
}

FReply SGraphPanel::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if( IsEditable.Get() )
	{
		if( InKeyEvent.GetKey() == EKeys::Up  ||InKeyEvent.GetKey() ==  EKeys::NumPadEight )
		{
			UpdateSelectedNodesPositions(FVector2D(0.0f,-GetSnapGridSize()));
			return FReply::Handled();
		}
		if( InKeyEvent.GetKey() ==  EKeys::Down || InKeyEvent.GetKey() ==  EKeys::NumPadTwo )
		{
			UpdateSelectedNodesPositions(FVector2D(0.0f,GetSnapGridSize()));
			return FReply::Handled();
		}
		if( InKeyEvent.GetKey() ==  EKeys::Right || InKeyEvent.GetKey() ==  EKeys::NumPadSix )
		{
			UpdateSelectedNodesPositions(FVector2D(GetSnapGridSize(),0.0f));
			return FReply::Handled();
		}
		if( InKeyEvent.GetKey() ==  EKeys::Left || InKeyEvent.GetKey() ==  EKeys::NumPadFour )
		{
			UpdateSelectedNodesPositions(FVector2D(-GetSnapGridSize(),0.0f));
			return FReply::Handled();
		}
		if(InKeyEvent.GetKey() == FGraphEditorCommands::Get().ZoomOut->GetActiveChord()->Key)
		{
			ChangeZoomLevel(-1, CachedAllottedGeometryScaledSize / 2.f, InKeyEvent.IsControlDown());
			return FReply::Handled();
		}
		if(InKeyEvent.GetKey() == FGraphEditorCommands::Get().ZoomIn->GetActiveChord()->Key)
		{
			ChangeZoomLevel(+1, CachedAllottedGeometryScaledSize / 2.f, InKeyEvent.IsControlDown());
			return FReply::Handled();
		}
	}

	return SNodePanel::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SGraphPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && (MouseEvent.IsAltDown() || MouseEvent.IsControlDown()))
	{
		if (SGraphPin* BestPinFromHoveredSpline = GetBestPinFromHoveredSpline())
		{
			return BestPinFromHoveredSpline->OnPinMouseDown(MyGeometry, MouseEvent);
		}
	}

	return SNodePanel::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SGraphPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && (MouseEvent.IsShiftDown()))
	{
		if (SGraphPin* BestPinFromHoveredSpline = GetBestPinFromHoveredSpline())
		{
			return BestPinFromHoveredSpline->OnMouseButtonUp(MyGeometry, MouseEvent);
		}
	}

	return SNodePanel::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SGraphPanel::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UEdGraphPin* Pin1;
	UEdGraphPin* Pin2;
	if (PreviousFrameSplineOverlap.GetPins(*this, /*out*/ Pin1, /*out*/ Pin2))
	{
		// Give the schema a chance to do something interesting with a double click on a proper spline (both ends are attached to a pin, i.e., not a preview/drag one)
		const FVector2D DoubleClickPositionInGraphSpace = PanelCoordToGraphCoord(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));

		const UEdGraphSchema* Schema = GraphObj->GetSchema();
		Schema->OnPinConnectionDoubleCicked(Pin1, Pin2, DoubleClickPositionInGraphSpace);
	}

	return SNodePanel::OnMouseButtonDoubleClick(MyGeometry, MouseEvent);
}

class SGraphPin* SGraphPanel::GetBestPinFromHoveredSpline() const
{
	TSharedPtr<SGraphPin> BestPinWidget = PreviousFrameSplineOverlap.GetBestPinWidget(*this);
	return BestPinWidget.Get();
}

void SGraphPanel::GetAllPins(TSet< TSharedRef<SWidget> >& AllPins)
{
	// Get the set of pins for all children
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(Children[ChildIndex]);
		ChildNode->GetPins(AllPins);
	}
}

void SGraphPanel::AddPinToHoverSet(UEdGraphPin* HoveredPin)
{
	CurrentHoveredPins.Add(HoveredPin);
	TimeWhenMouseEnteredPin = FSlateApplication::Get().GetCurrentTime();
}

void SGraphPanel::RemovePinFromHoverSet(UEdGraphPin* UnhoveredPin)
{
	CurrentHoveredPins.Remove(UnhoveredPin);
	TimeWhenMouseLeftPin = FSlateApplication::Get().GetCurrentTime();
}

void SGraphPanel::ArrangeChildrenForContextMenuSummon(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	// First pass nodes
	for (int32 ChildIndex = 0; ChildIndex < VisibleChildren.Num(); ++ChildIndex)
	{
		const TSharedRef<SNode>& SomeChild = VisibleChildren[ChildIndex];
		if (!SomeChild->RequiresSecondPassLayout())
		{
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(SomeChild, SomeChild->GetPosition() - ViewOffset, SomeChild->GetDesiredSizeForMarquee(), GetZoomAmount()));
		}
	}

	// Second pass nodes
	for (int32 ChildIndex = 0; ChildIndex < VisibleChildren.Num(); ++ChildIndex)
	{
		const TSharedRef<SNode>& SomeChild = VisibleChildren[ChildIndex];
		if (SomeChild->RequiresSecondPassLayout())
		{
			SomeChild->PerformSecondPassLayout(NodeToWidgetLookup);
			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(SomeChild, SomeChild->GetPosition() - ViewOffset, SomeChild->GetDesiredSizeForMarquee(), GetZoomAmount()));
		}
	}
}

TSharedPtr<SWidget> SGraphPanel::OnSummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	//Editability is up to the user to consider for menu options
	{
		// If we didn't drag very far, summon a context menu.
		// Figure out what's under the mouse: Node, Pin or just the Panel, and summon the context menu for that.
		UEdGraphNode* NodeUnderCursor = nullptr;
		UEdGraphPin* PinUnderCursor = nullptr;
		{
			FArrangedChildren ArrangedNodes(EVisibility::Visible);
			this->ArrangeChildrenForContextMenuSummon(MyGeometry, ArrangedNodes);
			const int32 HoveredNodeIndex = SWidget::FindChildUnderMouse( ArrangedNodes, MouseEvent );
			if (HoveredNodeIndex != INDEX_NONE)
			{
				const FArrangedWidget& HoveredNode = ArrangedNodes[HoveredNodeIndex];
				TSharedRef<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(HoveredNode.Widget);
				TSharedPtr<SGraphNode> GraphSubNode = GraphNode->GetNodeUnderMouse(HoveredNode.Geometry, MouseEvent);
				GraphNode = GraphSubNode.IsValid() ? GraphSubNode.ToSharedRef() : GraphNode;
				NodeUnderCursor = GraphNode->GetNodeObj();

				// Selection should switch to this code if it isn't already selected.
				// When multiple nodes are selected, we do nothing, provided that the
				// node for which the context menu is being created is in the selection set.
				if (!SelectionManager.IsNodeSelected(GraphNode->GetObjectBeingDisplayed()))
				{
					SelectionManager.SelectSingleNode(GraphNode->GetObjectBeingDisplayed());
				}

				const TSharedPtr<SGraphPin> HoveredPin = GraphNode->GetHoveredPin( HoveredNode.Geometry, MouseEvent );
				if (HoveredPin.IsValid())
				{
					PinUnderCursor = HoveredPin->GetPinObj();
				}
			}				
		}

		const FVector2D NodeAddPosition = PanelCoordToGraphCoord( MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()) );
		TArray<UEdGraphPin*> NoSourcePins;

		return SummonContextMenu(MouseEvent.GetScreenSpacePosition(), NodeAddPosition, NodeUnderCursor, PinUnderCursor, NoSourcePins, MouseEvent.IsShiftDown());
	}

	return TSharedPtr<SWidget>();
}


bool SGraphPanel::OnHandleLeftMouseRelease(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SGraphPin> PreviewConnectionPin = PreviewConnectorFromPins.Num() > 0 ? PreviewConnectorFromPins[0].FindInGraphPanel(*this) : nullptr;
	if (PreviewConnectionPin.IsValid() && IsEditable.Get())
	{
		TSet< TSharedRef<SWidget> > AllConnectors;
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			//@FINDME:
			TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(Children[ChildIndex]);
			ChildNode->GetPins(AllConnectors);
		}

		TMap<TSharedRef<SWidget>, FArrangedWidget> PinGeometries;
		this->FindChildGeometries(MyGeometry, AllConnectors, PinGeometries);

		bool bHandledDrop = false;
		TSet<UEdGraphNode*> NodeList;
		for ( TMap<TSharedRef<SWidget>, FArrangedWidget>::TIterator SomePinIt(PinGeometries); !bHandledDrop && SomePinIt; ++SomePinIt )
		{
			FArrangedWidget& PinWidgetGeometry = SomePinIt.Value();
			if( PinWidgetGeometry.Geometry.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
			{
				SGraphPin& TargetPin = static_cast<SGraphPin&>( PinWidgetGeometry.Widget.Get() );

				if (PreviewConnectionPin->TryHandlePinConnection(TargetPin))
				{
					NodeList.Add(TargetPin.GetPinObj()->GetOwningNode());
					NodeList.Add(PreviewConnectionPin->GetPinObj()->GetOwningNode());
				}
				bHandledDrop = true;
			}
		}

		// No longer make a connection for a pin; we just connected or failed to connect.
		OnStopMakingConnection(/*bForceStop=*/ true);

		return true;
	}
	else
	{
		return false;
	}
}

FReply SGraphPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	LastPointerEvent = MouseEvent;
	LastPointerGeometry = MyGeometry;

	// Save the mouse position to use in OnPaint for spline hit detection
	SavedMousePosForOnPaintEventLocalSpace = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// Invalidate the spline results if we moved very far
	const FVector2D MouseDelta = SavedMousePosForOnPaintEventLocalSpace - PreviousFrameSavedMousePosForSplineOverlap;
	const float MouseDeltaLengthSquared = MouseDelta.SizeSquared();
	const bool bCursorInDeadZone = MouseDeltaLengthSquared <= FMath::Square(FSlateApplication::Get().GetDragTriggerDistance());

	if (!bCursorInDeadZone)
	{
		//@TODO: Should we do this or just rely on the next OnPaint?
		// Our frame-latent approximation is going to be totally junk if the mouse is moving quickly
		OnSplineHoverStateChanged(FGraphSplineOverlapResult());
	}

	return SNodePanel::OnMouseMove(MyGeometry, MouseEvent);
}

void SGraphPanel::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FGraphEditorDragDropAction> DragConnectionOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
	if (DragConnectionOp.IsValid())
	{
		DragConnectionOp->SetHoveredGraph( SharedThis(this) );
	}
}

void SGraphPanel::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FGraphEditorDragDropAction> Operation = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
	if( Operation.IsValid() )
	{
		Operation->SetHoveredGraph(TSharedPtr<SGraphPanel>(nullptr));
	}
	else
	{
		TSharedPtr<FDecoratedDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
		if( AssetOp.IsValid()  )
		{
			AssetOp->ResetToDefaultToolTip();
		}
	}
}

FReply SGraphPanel::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return FReply::Unhandled();
	}

	// Handle Read only graphs
	if (!IsEditable.Get())
	{
		TSharedPtr<FGraphEditorDragDropAction> GraphDragDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();

		if (GraphDragDropOp.IsValid())
		{
			GraphDragDropOp->SetDropTargetValid(false);
		}
		else
		{
			TSharedPtr<FDecoratedDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
			if (AssetOp.IsValid())
			{
				FText Tooltip = AssetOp->GetHoverText();
				if (Tooltip.IsEmpty())
				{
					Tooltip = NSLOCTEXT( "GraphPanel", "DragDropOperation", "Graph is Read-Only" );
				}
				AssetOp->SetToolTip(Tooltip, FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
			}
		}
		return FReply::Handled();
	}

	if (Operation->IsOfType<FGraphEditorDragDropAction>())
	{
		PreviewConnectorEndpoint = MyGeometry.AbsoluteToLocal( DragDropEvent.GetScreenSpacePosition() );
		return FReply::Handled();
	}
	else if (Operation->IsOfType<FExternalDragOperation>())
	{
		return AssetUtil::CanHandleAssetDrag(DragDropEvent);
	}
	else if (Operation->IsOfType<FAssetDragDropOp>())
	{
		if ((GraphObj != nullptr) && (GraphObj->GetSchema() != nullptr))
		{
			TSharedPtr<FAssetDragDropOp> AssetOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
			bool bOkIcon = false;
			FString TooltipText;
			GraphObj->GetSchema()->GetAssetsGraphHoverMessage(AssetOp->AssetData, GraphObj, TooltipText, bOkIcon);
			const FSlateBrush* TooltipIcon = bOkIcon ? FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")) : FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));;
			AssetOp->SetToolTip(FText::FromString(TooltipText), TooltipIcon);
		}
		return FReply::Handled();
	} 
	else
	{
		return FReply::Unhandled();
	}
}

FReply SGraphPanel::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	const FVector2D NodeAddPosition = PanelCoordToGraphCoord( MyGeometry.AbsoluteToLocal( DragDropEvent.GetScreenSpacePosition() ) );

	FSlateApplication::Get().SetKeyboardFocus(AsShared(), EFocusCause::SetDirectly);

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid() || !IsEditable.Get())
	{
		return FReply::Unhandled();
	}

	if (Operation->IsOfType<FGraphEditorDragDropAction>())
	{
		check(GraphObj);
		TSharedPtr<FGraphEditorDragDropAction> DragConn = StaticCastSharedPtr<FGraphEditorDragDropAction>(Operation);
		if (DragConn.IsValid() && DragConn->IsSupportedBySchema(GraphObj->GetSchema()))
		{
			return DragConn->DroppedOnPanel(SharedThis(this), DragDropEvent.GetScreenSpacePosition(), NodeAddPosition, *GraphObj);
		}

		return FReply::Unhandled();
	}
	else if (Operation->IsOfType<FActorDragDropGraphEdOp>())
	{
		TSharedPtr<FActorDragDropGraphEdOp> ActorOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>(Operation);
		OnDropActor.ExecuteIfBound(ActorOp->Actors, GraphObj, NodeAddPosition);
		return FReply::Handled();
	}

	else if (Operation->IsOfType<FLevelDragDropOp>())
	{
		TSharedPtr<FLevelDragDropOp> LevelOp = StaticCastSharedPtr<FLevelDragDropOp>(Operation);
		OnDropStreamingLevel.ExecuteIfBound(LevelOp->StreamingLevelsToDrop, GraphObj, NodeAddPosition);
		return FReply::Handled();
	}
	else
	{
		if ((GraphObj != nullptr) && (GraphObj->GetSchema() != nullptr))
		{
			TArray< FAssetData > DroppedAssetData = AssetUtil::ExtractAssetDataFromDrag( DragDropEvent );

			if ( DroppedAssetData.Num() > 0 )
			{
				GraphObj->GetSchema()->DroppedAssetsOnGraph( DroppedAssetData, NodeAddPosition, GraphObj );
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}
}

void SGraphPanel::OnBeginMakingConnection(UEdGraphPin* InOriginatingPin)
{
	if (InOriginatingPin != nullptr)
	{
		PreviewConnectorFromPins.Add(InOriginatingPin);
	}
}

void SGraphPanel::OnStopMakingConnection(bool bForceStop)
{
	if (bForceStop || !bPreservePinPreviewConnection)
	{
		PreviewConnectorFromPins.Reset();
		bPreservePinPreviewConnection = false;
	}
}

void SGraphPanel::PreservePinPreviewUntilForced()
{
	bPreservePinPreviewConnection = true;
}

/** Add a slot to the CanvasPanel dynamically */
void SGraphPanel::AddGraphNode( const TSharedRef<SNodePanel::SNode>& NodeToAdd )
{
	TSharedRef<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(NodeToAdd);
	GraphNode->SetOwner( SharedThis(this) );

	const UEdGraphNode* Node = GraphNode->GetNodeObj();
	if (Node)
	{
		NodeGuidMap.Add(Node->NodeGuid, GraphNode);
	}

	SNodePanel::AddGraphNode(NodeToAdd);
}

void SGraphPanel::RemoveAllNodes()
{
	NodeGuidMap.Empty();
	CurrentHoveredPins.Empty();
	SNodePanel::RemoveAllNodes();
}

TSharedPtr<SWidget> SGraphPanel::SummonContextMenu(const FVector2D& WhereToSummon, const FVector2D& WhereToAddNode, UEdGraphNode* ForNode, UEdGraphPin* ForPin, const TArray<UEdGraphPin*>& DragFromPins, bool bShiftOperation)
{
	if (OnGetContextMenuFor.IsBound())
	{
		FGraphContextMenuArguments SpawnInfo;
		SpawnInfo.NodeAddPosition = WhereToAddNode;
		SpawnInfo.GraphNode = ForNode;
		SpawnInfo.GraphPin = ForPin;
		SpawnInfo.DragFromPins = DragFromPins;
		SpawnInfo.bShiftOperation = bShiftOperation;

		FActionMenuContent FocusedContent = OnGetContextMenuFor.Execute(SpawnInfo);

		TSharedRef<SWidget> MenuContent = FocusedContent.Content;
		
		FSlateApplication::Get().PushMenu(
			AsShared(),
			MenuContent,
			WhereToSummon,
			FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
			);

		return FocusedContent.WidgetToFocus;
	}

	return TSharedPtr<SWidget>();
}

void SGraphPanel::AttachGraphEvents(TSharedPtr<SGraphNode> CreatedSubNode)
{
	check(CreatedSubNode.IsValid());
	CreatedSubNode->SetIsEditable(IsEditable);
	CreatedSubNode->SetDoubleClickEvent(OnNodeDoubleClicked);
	CreatedSubNode->SetVerifyTextCommitEvent(OnVerifyTextCommit);
	CreatedSubNode->SetTextCommittedEvent(OnTextCommitted);
}

const TSharedRef<SGraphNode> SGraphPanel::GetChild(int32 ChildIndex)
{
	return StaticCastSharedRef<SGraphNode>(Children[ChildIndex]);
}

void SGraphPanel::AddNode(UEdGraphNode* Node, AddNodeBehavior Behavior)
{
	TSharedPtr<SGraphNode> NewNode = FNodeFactory::CreateNodeWidget(Node);
	check(NewNode.IsValid());

	const bool bWasUserAdded = 
		Behavior == WasUserAdded ? true : 
		Behavior == NotUserAdded ? false :
					(UserAddedNodes.Find(Node) != nullptr);

	NewNode->SetIsEditable(IsEditable);
	NewNode->SetDoubleClickEvent(OnNodeDoubleClicked);
	NewNode->SetVerifyTextCommitEvent(OnVerifyTextCommit);
	NewNode->SetTextCommittedEvent(OnTextCommitted);
	NewNode->SetDisallowedPinConnectionEvent(OnDisallowedPinConnection);

	this->AddGraphNode
	(
		NewNode.ToSharedRef()
	);

	if (bWasUserAdded)
	{
		// Add the node to visible children, this allows focus to occur on sub-widgets for naming purposes.
		VisibleChildren.Add(NewNode.ToSharedRef());

		NewNode->PlaySpawnEffect();
		NewNode->RequestRenameOnSpawn();
	}
}

void SGraphPanel::RemoveNode(const UEdGraphNode* Node)
{
	for (int32 Iter = 0; Iter != Children.Num(); ++Iter)
	{
		TSharedRef<SGraphNode> Child = GetChild(Iter);
		if (Child->GetNodeObj() == Node)
		{
			Children.RemoveAt(Iter);
			break;
		}
	}
	for (int32 Iter = 0; Iter != VisibleChildren.Num(); ++Iter)
	{
		TSharedRef<SGraphNode> Child = StaticCastSharedRef<SGraphNode>(VisibleChildren[Iter]);
		if (Child->GetNodeObj() == Node)
		{
			VisibleChildren.RemoveAt(Iter);
			break;
		}
	}
}

TSharedPtr<SGraphNode> SGraphPanel::GetNodeWidgetFromGuid(FGuid Guid) const
{
	return NodeGuidMap.FindRef(Guid).Pin();
}

void SGraphPanel::Update()
{
	// Add widgets for all the nodes that don't have one.
	if (GraphObj != nullptr)
	{
		// Scan for all missing nodes
		for (int32 NodeIndex = 0; NodeIndex < GraphObj->Nodes.Num(); ++NodeIndex)
		{
			UEdGraphNode* Node = GraphObj->Nodes[NodeIndex];
			if (Node)
			{
				AddNode(Node, CheckUserAddedNodesList);
			}
			else
			{
				UE_LOG(LogGraphPanel, Warning, TEXT("Found NULL Node in GraphObj array. A node type has been deleted without creating an ActiveClassRedictor to K2Node_DeadClass."));
			}
		}

		// find the last selection action, and execute it
		for (int32 ActionIndex = UserActions.Num() - 1; ActionIndex >= 0; --ActionIndex)
		{
			if (UserActions[ActionIndex].Action & GRAPHACTION_SelectNode)
			{
				DeferredSelectionTargetObjects.Empty();
				for (const UEdGraphNode* Node : UserActions[ActionIndex].Nodes)
				{
					DeferredSelectionTargetObjects.Add(Node);
				}
				break;
			}
		}
	}
	else
	{
		RemoveAllNodes();
	}

	// Clean out set of added nodes
	UserAddedNodes.Empty();
	UserActions.Empty();

	// Invoke any delegate methods
	OnUpdateGraphPanel.ExecuteIfBound();
}

// Purges the existing visual representation (typically followed by an Update call in the next tick)
void SGraphPanel::PurgeVisualRepresentation()
{
	// No need to call OnSplineHoverStateChanged since we're about to destroy all the nodes and pins
	PreviousFrameSplineOverlap = FGraphSplineOverlapResult();

	// Clear all of the nodes and pins
	RemoveAllNodes();
}

bool SGraphPanel::IsNodeTitleVisible(const class UEdGraphNode* Node, bool bRequestRename)
{
	bool bTitleVisible = false;
	TSharedRef<SNode>* pWidget = NodeToWidgetLookup.Find(Node);

	if (pWidget != nullptr)
	{
		TWeakPtr<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(*pWidget);
		if(GraphNode.IsValid() && !HasMouseCapture())
		{
			FSlateRect TitleRect = GraphNode.Pin()->GetTitleRect();
			const FVector2D TopLeft = FVector2D( TitleRect.Left, TitleRect.Top );
			const FVector2D BottomRight = FVector2D( TitleRect.Right, TitleRect.Bottom );

			if( IsRectVisible( TopLeft, BottomRight ))
			{
				bTitleVisible = true;
			}
			else if( bRequestRename )
			{
				bTitleVisible = JumpToRect( TopLeft, BottomRight );
			}

			if( bTitleVisible && bRequestRename )
			{
				GraphNode.Pin()->RequestRename();
				SelectAndCenterObject(Node, false);
			}
		}
	}
	return bTitleVisible;
}

bool SGraphPanel::IsRectVisible(const FVector2D &TopLeft, const FVector2D &BottomRight)
{
	return TopLeft >= PanelCoordToGraphCoord( FVector2D::ZeroVector ) && BottomRight <= PanelCoordToGraphCoord( CachedAllottedGeometryScaledSize );
}

bool SGraphPanel::JumpToRect(const FVector2D &TopLeft, const FVector2D &BottomRight)
{
	ZoomToTarget(TopLeft, BottomRight);

	return true;
}

void SGraphPanel::JumpToNode(const UEdGraphNode* JumpToMe, bool bRequestRename)
{
	if (JumpToMe != nullptr)
	{
		if (bRequestRename)
		{
			TSharedRef<SNode>* pWidget = NodeToWidgetLookup.Find(JumpToMe);
			if (pWidget != nullptr)
			{
				TSharedRef<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(*pWidget);
				GraphNode->RequestRename();
			}
		}

		// Select this node, and request that we jump to it.
		SelectAndCenterObject(JumpToMe, true);
	}
}

void SGraphPanel::JumpToPin(const UEdGraphPin* JumpToMe)
{
	if (JumpToMe != nullptr)
	{
		JumpToNode(JumpToMe->GetOwningNode(), false);
	}
}

void SGraphPanel::OnBeginPIE( const bool bIsSimulating )
{
	// Play the bounce curve on a continuous loop during PIE
	BounceCurve.Play( this->AsShared(), true );
}

void SGraphPanel::OnEndPIE( const bool bIsSimulating )
{
	// Stop the bounce curve
	BounceCurve.JumpToEnd();
}

void SGraphPanel::OnGraphChanged(const FEdGraphEditAction& EditAction)
{
	const bool bWillPurge = GraphObj->GetSchema()->ShouldAlwaysPurgeOnModification();
	if (bWillPurge)
	{
		if ((EditAction.Graph == GraphObj) &&
			(EditAction.Nodes.Num() > 0) &&
			EditAction.bUserInvoked)
		{
			int32 ActionIndex = UserActions.Num();
			if (EditAction.Action & GRAPHACTION_AddNode)
			{
				for (const UEdGraphNode* Node : EditAction.Nodes)
				{
					UserAddedNodes.Add(Node, ActionIndex);
				}
			}
			UserActions.Add(EditAction);
		}
	}
	else if ((EditAction.Graph == GraphObj) && (EditAction.Nodes.Num() > 0) )
	{
		// Remove action handled immediately by SGraphPanel::OnGraphChanged
		const bool bWasAddAction = (EditAction.Action & GRAPHACTION_AddNode) != 0;
		const bool bWasSelectAction = (EditAction.Action & GRAPHACTION_SelectNode) != 0;
		const bool bWasRemoveAction = (EditAction.Action & GRAPHACTION_RemoveNode) != 0;

		// The *only* reason we defer these actions is because code higher up the call stack
		// assumes that the node is created later (for example, GenerateBlueprintAPIUtils::AddNodeToGraph
		// calls AddNode (which calls this function) before calling AllocateDefaultPins, so if we create 
		// the widget immediately it won't be able to create its pins). There are lots of other examples, 
		// and I can't be sure that I've found them all.... 
		
		// Minor note, the ugly little lambdas are just to deal with the  time values and return values 
		// that the timer system requires (and we don't leverage):
		if (bWasRemoveAction)
		{
			const auto RemoveNodeDelegateWrapper = [](double, float, SGraphPanel* Parent, const UEdGraphNode* Node) -> EActiveTimerReturnType
			{
				Parent->RemoveNode(Node);
				return EActiveTimerReturnType::Stop;
			};

			for (auto Node : EditAction.Nodes)
			{
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateStatic(RemoveNodeDelegateWrapper, this, Node));
			}
		}
		if (bWasAddAction)
		{
			const auto AddNodeDelegateWrapper = [](double, float, SGraphPanel* Parent, UEdGraphNode* Node, bool bForceUserAdded) -> EActiveTimerReturnType
			{
				Parent->RemoveNode(Node);
				Parent->AddNode(Node, bForceUserAdded ? WasUserAdded : NotUserAdded );
				return EActiveTimerReturnType::Stop;
			};

			for (auto Node : EditAction.Nodes)
			{
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateStatic(AddNodeDelegateWrapper, this, const_cast<UEdGraphNode*>(Node), EditAction.bUserInvoked ) );
			}
		}
		if (bWasSelectAction)
		{
			const auto SelectNodeDelegateWrapper = [](double, float, SGraphPanel* Parent, TSet<const UEdGraphNode*> Nodes) -> EActiveTimerReturnType
			{
				Parent->DeferredSelectionTargetObjects.Empty();
				for (auto Node : Nodes)
				{
					Parent->DeferredSelectionTargetObjects.Add(Node);
				}
				return EActiveTimerReturnType::Stop;
			};
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateStatic(SelectNodeDelegateWrapper, this, EditAction.Nodes));
		}
	}
}

void SGraphPanel::NotifyGraphChanged(const FEdGraphEditAction& EditAction)
{
	// Forward call
	OnGraphChanged(EditAction);
}

void SGraphPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject( GraphObj );
	Collector.AddReferencedObject( GraphObjToDiff );
}
