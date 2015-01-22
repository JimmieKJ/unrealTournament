// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "SPaperEditorViewport.h"
#include "PaperEditorViewportClient.h"

const int32 DefaultZoomLevel = 5;
const int32 NumZoomLevels = 13;

struct FZoomLevelEntry
{
	FZoomLevelEntry(float InZoomAmount, const FText& InDisplayText)
		: DisplayText(FText::Format(NSLOCTEXT("PaperEditor", "Zoom", "Zoom {0}"), InDisplayText))
		, ZoomAmount(InZoomAmount)
	{
	}

	FText DisplayText;
	float ZoomAmount;
};

static const FZoomLevelEntry ZoomLevels[NumZoomLevels] =
{
	FZoomLevelEntry(0.125f, NSLOCTEXT("PaperEditor", "ZoomLevel", "1:8")),
	FZoomLevelEntry(0.250f, NSLOCTEXT("PaperEditor", "ZoomLevel", "1:4")),
	FZoomLevelEntry(0.500f, NSLOCTEXT("PaperEditor", "ZoomLevel", "1:2")),
	FZoomLevelEntry(0.750f, NSLOCTEXT("PaperEditor", "ZoomLevel", "3:4")),
	FZoomLevelEntry(0.875f, NSLOCTEXT("PaperEditor", "ZoomLevel", "7:8")),
	FZoomLevelEntry(1.000f, NSLOCTEXT("PaperEditor", "ZoomLevel", "1:1")),
	FZoomLevelEntry(2.000f, NSLOCTEXT("PaperEditor", "ZoomLevel", "2x")),
	FZoomLevelEntry(3.000f, NSLOCTEXT("PaperEditor", "ZoomLevel", "3x")),
	FZoomLevelEntry(4.500f, NSLOCTEXT("PaperEditor", "ZoomLevel", "4x")),
	FZoomLevelEntry(5.000f, NSLOCTEXT("PaperEditor", "ZoomLevel", "5x")),
	FZoomLevelEntry(6.000f, NSLOCTEXT("PaperEditor", "ZoomLevel", "6x")),
	FZoomLevelEntry(7.000f, NSLOCTEXT("PaperEditor", "ZoomLevel", "7x")),
	FZoomLevelEntry(8.000f, NSLOCTEXT("PaperEditor", "ZoomLevel", "8x"))
};

//////////////////////////////////////////////////////////////////////////
// SPaperEditorViewport

SPaperEditorViewport::~SPaperEditorViewport()
{
	// Close viewport
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}

	Viewport.Reset();
	ViewportClient.Reset();
}

void SPaperEditorViewport::Construct(const FArguments& InArgs, TSharedRef<FPaperEditorViewportClient> InViewportClient)
{
	OnSelectionChanged = InArgs._OnSelectionChanged;

	this->ChildSlot
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SAssignNew(ViewportWidget, SViewport)
			//.Visibility(EVisibility::HitTestInvisible)
			.EnableGammaCorrection(false)
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
			.ShowEffectWhenDisabled(false)
		]
		// Indicator of current zoom level
		+SOverlay::Slot()
		.Padding(5)
		.VAlign(VAlign_Top)
		[
			SNew(STextBlock)
			.Font(FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-BoldCondensed"), 16))
			.Text(this, &SPaperEditorViewport::GetZoomText)
			.ColorAndOpacity(this, &SPaperEditorViewport::GetZoomTextColorAndOpacity)
		]
		+SOverlay::Slot()
		.VAlign(VAlign_Top)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( TEXT("Graph.TitleBackground") ) )
			.HAlign(HAlign_Fill)
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SVerticalBox)
				// Title text/icon
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.FillWidth(1.f)
					[
						SNew(STextBlock)
						.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18 ) )
						.ColorAndOpacity( FLinearColor(1,1,1,0.5) )
						.Text( this, &SPaperEditorViewport::GetTitleText )
					]
				]
			]
		]
	];

	ViewportClient = InViewportClient;
	ViewportClient->SetRealtime(false);

	Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));
	ViewportClient->Viewport = Viewport.Get();

	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface( Viewport.ToSharedRef() );


	ZoomLevel = DefaultZoomLevel;
	PreviousZoomLevel = DefaultZoomLevel;
	ViewOffset = FVector2D::ZeroVector;
	TotalMouseDelta = 0;
	//bAllowContinousZoomInterpolation = false;

	bIsPanning = false;

	ZoomLevelFade = FCurveSequence( 0.0f, 0.75f );
	ZoomLevelFade.Play();

	ZoomLevelGraphFade = FCurveSequence( 0.0f, 0.5f );
	ZoomLevelGraphFade.Play();

	DeferredPanPosition = FVector2D::ZeroVector;
	bRequestDeferredPan = false;
}

void SPaperEditorViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Handle any deferred panning
	if (bRequestDeferredPan)
	{
		bRequestDeferredPan = false;
		UpdateViewOffset(AllottedGeometry, DeferredPanPosition);
	}

	if (!HasMouseCapture())
	{
		bShowSoftwareCursor = false;
		bIsPanning = false;
	}


	ViewportClient->SetZoomPos(ViewOffset, GetZoomAmount());
	ViewportClient->bNeedsRedraw = true;

	bool bSelectionModified = false;
	if (Marquee.IsValid())
	{
		bSelectionModified = true;

		OnSelectionChanged.ExecuteIfBound(Marquee, true);
	}

	if (bSelectionModified || bIsPanning || FSlateThrottleManager::Get().IsAllowingExpensiveTasks())
	{
		// Setup the selection set for the viewport
		ViewportClient->SelectionRectangles.Empty();

		if (Marquee.IsValid())
		{
			FViewportSelectionRectangle& Rect = *(new (ViewportClient->SelectionRectangles) FViewportSelectionRectangle);
			Rect.Color = FColorList::Yellow;
			Rect.Color.A = 0.45f;
			Rect.TopLeft = Marquee.Rect.GetUpperLeft();
			Rect.Dimensions = Marquee.Rect.GetSize();
		}

		// Tick and render the viewport
		ViewportClient->Tick(InDeltaTime);
		GEditor->UpdateSingleViewportClient(ViewportClient.Get(), /*bInAllowNonRealtimeViewportToDraw=*/ true, /*bLinkedOrthoMovement=*/ false);
	}

	//
	SWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

FReply SPaperEditorViewport::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TotalMouseDelta = 0;

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// RIGHT BUTTON is for dragging and Context Menu.
		FReply ReplyState = FReply::Handled();
		ReplyState.CaptureMouse( SharedThis(this) );
		ReplyState.UseHighPrecisionMouseMovement( SharedThis(this) );

		SoftwareCursorPosition = PanelCoordToGraphCoord( MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ) );

		// clear any interpolation when you manually pan
		//DeferredMovementTargetObject = nullptr;

		return ReplyState;
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// START MARQUEE SELECTION.
		const FVector2D GraphMousePos = PanelCoordToGraphCoord( MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ) );
		Marquee.Start( GraphMousePos, FMarqueeOperation::OperationTypeFromMouseEvent(MouseEvent) );

		// Trigger a selection update now so that single-clicks without a drag still select something
		OnSelectionChanged.ExecuteIfBound(Marquee, true);
		ViewportClient->Invalidate();

		return FReply::Handled().CaptureMouse( SharedThis(this) );
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SPaperEditorViewport::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Did the user move the cursor sufficiently far, or is it in a dead zone?
	// In Dead zone     - implies actions like summoning context menus and general clicking.
	// Out of Dead Zone - implies dragging actions like moving nodes and marquee selection.
	const bool bCursorInDeadZone = TotalMouseDelta <= FSlateApplication::Get().GetDragTriggerDistnace();

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FReply ReplyState = FReply::Handled();

		if (this->HasMouseCapture())
		{
			FSlateRect ThisPanelScreenSpaceRect = MyGeometry.GetClippingRect();
			const FVector2D ScreenSpaceCursorPos = MyGeometry.LocalToAbsolute( GraphCoordToPanelCoord( SoftwareCursorPosition ) );

			FIntPoint BestPositionInViewport(
				FMath::RoundToInt( FMath::Clamp( ScreenSpaceCursorPos.X, ThisPanelScreenSpaceRect.Left, ThisPanelScreenSpaceRect.Right ) ),
				FMath::RoundToInt( FMath::Clamp( ScreenSpaceCursorPos.Y, ThisPanelScreenSpaceRect.Top, ThisPanelScreenSpaceRect.Bottom ) )
				);

			if (!bCursorInDeadZone)
			{
				ReplyState.SetMousePos(BestPositionInViewport);
			}
		}

		TSharedPtr<SWidget> WidgetToFocus;
		if (bCursorInDeadZone)
		{
			//WidgetToFocus = OnSummonContextMenu(MyGeometry, MouseEvent);
		}

		bShowSoftwareCursor = false;

		this->bIsPanning = false;
		return (WidgetToFocus.IsValid())
			? ReplyState.ReleaseMouseCapture().SetUserFocus(WidgetToFocus.ToSharedRef(), EFocusCause::SetDirectly)
			: ReplyState.ReleaseMouseCapture();
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (OnHandleLeftMouseRelease(MyGeometry, MouseEvent))
		{

		}
		else if (bCursorInDeadZone)
		{
			//@TODO: Move to selection manager
// 			if ( NodeUnderMousePtr.IsValid() )
// 			{
// 				// We clicked on a node!
// 				TSharedRef<SNode> NodeWidgetUnderMouse = NodeUnderMousePtr.Pin().ToSharedRef();
// 
// 				SelectionManager.ClickedOnNode(NodeWidgetUnderMouse->GetObjectBeingDisplayed(), MouseEvent);
// 
// 				// We're done interacting with this node.
// 				NodeUnderMousePtr.Reset();
// 			}
// 			else 
			if (this->HasMouseCapture())
 			{
 				// We clicked on the panel background

				//@TODO: There isn't a marquee operation for clear, need to signal to remove existing sets too!
				//Marquee.End();
 				//OnSelectionChanged.ExecuteIfBound(Marquee, false);
 			}
		}
		else if (Marquee.IsValid())
		{
			//ApplyMarqueeSelection(Marquee, SelectionManager.SelectedNodes, SelectionManager.SelectedNodes);
			//SelectionManager.OnSelectionChanged.ExecuteIfBound(SelectionManager.SelectedNodes);
			OnSelectionChanged.ExecuteIfBound(Marquee, true);
		}

		// The existing marquee operation ended; reset it.
		Marquee = FMarqueeOperation();

		if (bIsPanning)
		{
			// We have released the left mouse button. But we're still panning
			// (i.e. RIGHT MOUSE is down), so we want to hold on to capturing mouse input.
			return FReply::Handled();
		}
		else
		{
			// We aren't panning, so we can release the mouse capture.
			return FReply::Handled().ReleaseMouseCapture();
		}	
	}

	return FReply::Unhandled();	
}

FReply SPaperEditorViewport::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const bool bIsRightMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);
	const bool bIsLeftMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
	
	if (this->HasMouseCapture())
	{
		// Track how much the mouse moved since the mouse down.
		const FVector2D CursorDelta = MouseEvent.GetCursorDelta();
		TotalMouseDelta += CursorDelta.Size();

		if (bIsRightMouseButtonDown)
		{
			FReply ReplyState = FReply::Handled();

			if (!CursorDelta.IsZero())
			{
				bShowSoftwareCursor = true;
			}

			bIsPanning = true;
			ViewOffset -= CursorDelta / GetZoomAmount();

			return ReplyState;
		}
		else if (bIsLeftMouseButtonDown)
		{
//			TSharedPtr<SNode> NodeBeingDragged = NodeUnderMousePtr.Pin();

			// Update the amount to pan panel
			UpdateViewOffset(MyGeometry, MouseEvent.GetScreenSpacePosition());

			const bool bCursorInDeadZone = TotalMouseDelta <= FSlateApplication::Get().GetDragTriggerDistnace();

// 			if (NodeBeingDragged.IsValid())
// 			{
// 				if (!bCursorInDeadZone)
// 				{
// 					// Note, NodeGrabOffset() comes from the node itself, so it's already scaled correctly.
// 					FVector2D AnchorNodeNewPos = PanelCoordToGraphCoord( MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ) ) - NodeGrabOffset;
// 
// 					// Snap to grid
// 					const float SnapSize = GetSnapGridSize();
// 					AnchorNodeNewPos.X = SnapSize * FMath::RoundToInt(AnchorNodeNewPos.X/SnapSize);
// 					AnchorNodeNewPos.Y = SnapSize * FMath::RoundToInt(AnchorNodeNewPos.Y/SnapSize);
// 
// 					// Dragging an unselected node automatically selects it.
// 					SelectionManager.StartDraggingNode(NodeBeingDragged->GetObjectBeingDisplayed(), MouseEvent);
// 
// 					// Move all the selected nodes.
// 					{
// 						const FVector2D AnchorNodeOldPos = NodeBeingDragged->GetPosition();
// 						const FVector2D DeltaPos = AnchorNodeNewPos - AnchorNodeOldPos;
// 
// 						for (FGraphPanelSelectionSet::TIterator NodeIt(SelectionManager.SelectedNodes); NodeIt; ++NodeIt)
// 						{
// 							TSharedRef<SNode>* pWidget = NodeToWidgetLookup.Find(*NodeIt);
// 							if (pWidget != nullptr)
// 							{
// 								SNode& Widget = pWidget->Get();
// 								Widget.MoveTo(Widget.GetPosition() + DeltaPos);
// 							}
// 						}
// 					}
// 				}
// 
// 				return FReply::Handled();
// 			}
// 
// 			if (!NodeBeingDragged.IsValid())
			{
				// We are marquee selecting
				const FVector2D GraphMousePos = PanelCoordToGraphCoord( MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ) );
				Marquee.Rect.UpdateEndPoint(GraphMousePos);

//				FindNodesAffectedByMarquee( /*out*/ Marquee.AffectedNodes );
				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

FReply SPaperEditorViewport::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// We want to zoom into this point; i.e. keep it the same fraction offset into the panel
	const FVector2D WidgetSpaceCursorPos = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
	FVector2D PointToMaintainGraphSpace = PanelCoordToGraphCoord( WidgetSpaceCursorPos );


	const int32 ZoomLevelDelta = FMath::FloorToInt(MouseEvent.GetWheelDelta());

	const bool bAllowFullZoomRange = true;
/*
		// To zoom out past 1:1 the user must press control
		(ZoomLevel == DefaultZoomLevel && (ZoomLevelDelta > 0) && MouseEvent.IsControlDown()) ||
		// If they are already zoomed out past 1:1, user may zoom freely
		(ZoomLevel > DefaultZoomLevel);
*/
	const float OldZoomLevel = ZoomLevel;

	if (bAllowFullZoomRange)
	{
		ZoomLevel = FMath::Clamp( ZoomLevel + ZoomLevelDelta, 0, NumZoomLevels-1 );
	}
	else
	{
		// Without control, we do not allow zooming out past 1:1.
		ZoomLevel = FMath::Clamp( ZoomLevel + ZoomLevelDelta, 0, DefaultZoomLevel );
	}

	ZoomLevelFade.Play();


	// Re-center the screen so that it feels like zooming around the cursor.
	{
 		FSlateRect GraphBounds = ComputeSensibleGraphBounds();
 		// Make sure we are not zooming into/out into emptiness; otherwise the user will get lost..
 		const FVector2D ClampedPointToMaintainGraphSpace(
 			FMath::Clamp(PointToMaintainGraphSpace.X, GraphBounds.Left, GraphBounds.Right),
 			FMath::Clamp(PointToMaintainGraphSpace.Y, GraphBounds.Top, GraphBounds.Bottom)
 			);
		this->ViewOffset = ClampedPointToMaintainGraphSpace - WidgetSpaceCursorPos / GetZoomAmount();
	}

	return FReply::Handled();
}

FCursorReply SPaperEditorViewport::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	return bShowSoftwareCursor ? 
		FCursorReply::Cursor( EMouseCursor::None ) :
		FCursorReply::Cursor( EMouseCursor::Default );
}

void SPaperEditorViewport::RefreshViewport()
{
	Viewport->Invalidate();
	Viewport->InvalidateDisplay();
}

/** Search through the list of zoom levels and find the one closest to what was passed in. */
int32 SPaperEditorViewport::FindNearestZoomLevel(int32 CurrentZoomLevel, float InZoomAmount) const
{
	for (int32 ZoomLevelIndex=0; ZoomLevelIndex < NumZoomLevels; ++ZoomLevelIndex)
	{
		if (InZoomAmount <= ZoomLevels[ZoomLevelIndex].ZoomAmount)
		{
			return ZoomLevelIndex;
		}
	}

	return DefaultZoomLevel;
}


float SPaperEditorViewport::GetZoomAmount() const
{
	if (false)//bAllowContinousZoomInterpolation)
	{
		return FMath::Lerp(ZoomLevels[PreviousZoomLevel].ZoomAmount, ZoomLevels[ZoomLevel].ZoomAmount, ZoomLevelGraphFade.GetLerp());
	}
	else
	{
		return ZoomLevels[ZoomLevel].ZoomAmount;
	}
}

FText SPaperEditorViewport::GetZoomText() const
{
	return ZoomLevels[ZoomLevel].DisplayText;
}

FSlateColor SPaperEditorViewport::GetZoomTextColorAndOpacity() const
{
	return FLinearColor( 1, 1, 1, 1.25f - ZoomLevelFade.GetLerp() * 0.75f );
}

FVector2D SPaperEditorViewport::GetViewOffset() const
{
	return ViewOffset;
}

FVector2D SPaperEditorViewport::ComputeEdgePanAmount(const FGeometry& MyGeometry, const FVector2D& TargetPosition)
{
	// How quickly to ramp up the pan speed as the user moves the mouse further past the edge of the graph panel.
	static const float EdgePanSpeedCoefficient = 0.1f;

	// Never pan slower than this, it's just unpleasant.
	static const float MinPanSpeed = 5.0f;

	// Start panning before we rech the edge of the graph panel.
	static const float EdgePanForgivenessZone = 30.0f;

	const FVector2D LocalCursorPos = MyGeometry.AbsoluteToLocal( TargetPosition );

	// If the mouse is outside of the graph area, then we want to pan in that direction.
	// The farther out the mouse is, the more we want to pan.

	FVector2D EdgePanThisTick(0,0);
	if ( LocalCursorPos.X <= EdgePanForgivenessZone )
	{
		EdgePanThisTick.X += FMath::Min( -MinPanSpeed, EdgePanSpeedCoefficient * (EdgePanForgivenessZone - LocalCursorPos.X) );
	}
	else if( LocalCursorPos.X >= MyGeometry.Size.X - EdgePanForgivenessZone )
	{
		EdgePanThisTick.X = FMath::Max( MinPanSpeed, EdgePanSpeedCoefficient * (MyGeometry.Size.X - EdgePanForgivenessZone - LocalCursorPos.X) );
	}

	if ( LocalCursorPos.Y <= EdgePanForgivenessZone )
	{
		EdgePanThisTick.Y += FMath::Min( -MinPanSpeed, EdgePanSpeedCoefficient * (EdgePanForgivenessZone - LocalCursorPos.Y) );
	}
	else if( LocalCursorPos.Y >= MyGeometry.Size.Y - EdgePanForgivenessZone )
	{
		EdgePanThisTick.Y = FMath::Max( MinPanSpeed, EdgePanSpeedCoefficient * (MyGeometry.Size.Y - EdgePanForgivenessZone - LocalCursorPos.Y) );
	}

	return EdgePanThisTick;
}

void SPaperEditorViewport::UpdateViewOffset(const FGeometry& MyGeometry, const FVector2D& TargetPosition)
{
	const FVector2D PanAmount = ComputeEdgePanAmount( MyGeometry, TargetPosition ) / GetZoomAmount();
	ViewOffset += PanAmount;
}

void SPaperEditorViewport::RequestDeferredPan(const FVector2D& UpdatePosition)
{
	bRequestDeferredPan = true;
	DeferredPanPosition = UpdatePosition;
}

FVector2D SPaperEditorViewport::GraphCoordToPanelCoord( const FVector2D& GraphSpaceCoordinate ) const
{
	return (GraphSpaceCoordinate - GetViewOffset()) * GetZoomAmount();
}

FVector2D SPaperEditorViewport::PanelCoordToGraphCoord( const FVector2D& PanelSpaceCoordinate ) const
{
	return PanelSpaceCoordinate / GetZoomAmount() + GetViewOffset();
}

FSlateRect SPaperEditorViewport::PanelRectToGraphRect( const FSlateRect& PanelSpaceRect ) const
{
	FVector2D UpperLeft = PanelCoordToGraphCoord( FVector2D(PanelSpaceRect.Left, PanelSpaceRect.Top) );
	FVector2D LowerRight = PanelCoordToGraphCoord(  FVector2D(PanelSpaceRect.Right, PanelSpaceRect.Bottom) );

	return FSlateRect(
		UpperLeft.X, UpperLeft.Y,
		LowerRight.X, LowerRight.Y );
}

void SPaperEditorViewport::PaintSoftwareCursor(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 DrawLayerId) const
{
	if (bShowSoftwareCursor)
	{
		const FSlateBrush* Brush = FEditorStyle::GetBrush(TEXT("SoftwareCursor_Grab"));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			DrawLayerId,
			AllottedGeometry.ToPaintGeometry( GraphCoordToPanelCoord(SoftwareCursorPosition) - ( Brush->ImageSize / 2 ), Brush->ImageSize ),
			Brush,
			MyClippingRect);
	}
}

int32 SPaperEditorViewport::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	++MaxLayerId;
	PaintSoftwareCursor(AllottedGeometry, MyClippingRect, OutDrawElements, MaxLayerId);

	return MaxLayerId;
}

FSlateRect SPaperEditorViewport::ComputeSensibleGraphBounds() const
{
	float Left = 0.0f;
	float Top = 0.0f;

	//@TODO: PAPER: Use real values!
	float Right = 512.0f;//0.0f;
	float Bottom = 1024.0f;

	// Pad it out in every direction, to roughly account for nodes being of non-zero extent
	const float Padding = 100.0f;

	return FSlateRect( Left - Padding, Top - Padding, Right + Padding, Bottom + Padding );
}

FText SPaperEditorViewport::GetTitleText() const
{
	return NSLOCTEXT("PaperEditor", "TileSetPaletteTitle", "tile set palette");
}
