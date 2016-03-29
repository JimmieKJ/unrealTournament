// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerTimeSliderController.h"

#define LOCTEXT_NAMESPACE "TimeSlider"

namespace ScrubConstants
{
	/** The minimum amount of pixels between each major ticks on the widget */
	const int32 MinPixelsPerDisplayTick = 12;

	/**The smallest number of units between between major tick marks */
	const float MinDisplayTickSpacing = 0.001f;

	/**The fraction of the current view range to scroll per unit delta  */
	const float ScrollPanFraction = 0.1f;
}

/** Utility struct for converting between scrub range space and local/absolute screen space */
struct FScrubRangeToScreen
{
	FVector2D WidgetSize;

	TRange<float> ViewInput;
	float ViewInputRange;
	float PixelsPerInput;

	FScrubRangeToScreen(TRange<float> InViewInput, const FVector2D& InWidgetSize )
	{
		WidgetSize = InWidgetSize;

		ViewInput = InViewInput;
		ViewInputRange = ViewInput.Size<float>();
		PixelsPerInput = ViewInputRange > 0 ? ( WidgetSize.X / ViewInputRange ) : 0;
	}

	/** Local Widget Space -> Curve Input domain. */
	float LocalXToInput(float ScreenX) const
	{
		float LocalX = ScreenX;
		return (LocalX/PixelsPerInput) + ViewInput.GetLowerBoundValue();
	}

	/** Curve Input domain -> local Widget Space */
	float InputToLocalX(float Input) const
	{
		return (Input - ViewInput.GetLowerBoundValue()) * PixelsPerInput;
	}
};


/**
 * Gets the the next spacing value in the series 
 * to determine a good spacing value
 * E.g, .001,.005,.010,.050,.100,.500,1.000,etc
 */
static float GetNextSpacing( uint32 CurrentStep )
{
	if(CurrentStep & 0x01) 
	{
		// Odd numbers
		return FMath::Pow( 10.f, 0.5f*((float)(CurrentStep-1)) + 1.f );
	}
	else 
	{
		// Even numbers
		return 0.5f * FMath::Pow( 10.f, 0.5f*((float)(CurrentStep)) + 1.f );
	}
}

/**
 * Determines the optimal spacing between tick marks in the slider for a given pixel density
 * Increments until a minimum amount of slate units specified by MinTick is reached
 * 
 * @param InPixelsPerInput	The density of pixels between each input
 * @param MinTick			The minimum slate units per tick allowed
 * @param MinTickSpacing	The minimum tick spacing in time units allowed
 * @return the optimal spacing in time units
 */
float DetermineOptimalSpacing( float InPixelsPerInput, uint32 MinTick, float MinTickSpacing )
{
	uint32 CurStep = 0;

	// Start with the smallest spacing
	float Spacing = MinTickSpacing;

	if (InPixelsPerInput > 0)
	{
		while( Spacing * InPixelsPerInput < MinTick )
		{
			Spacing = MinTickSpacing * GetNextSpacing( CurStep );
			CurStep++;
		}
	}

	return Spacing;
}


FSequencerTimeSliderController::FSequencerTimeSliderController( const FTimeSliderArgs& InArgs )
	: TimeSliderArgs( InArgs )
	, DistanceDragged( 0.0f )
	, MouseDragType( DRAG_NONE )
	, bPanning( false )
{
	ScrubHandleUp = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.ScrubHandleUp" ) ); 
	ScrubHandleDown = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.ScrubHandleDown" ) );
	ScrubHandleSize = 13.f;
	ContextMenuSuppression = 0;
}

struct FDrawTickArgs
{
	/** Geometry of the area */
	FGeometry AllottedGeometry;
	/** Clipping rect of the area */
	FSlateRect ClippingRect;
	/** Color of each tick */
	FLinearColor TickColor;
	/** Offset in Y where to start the tick */
	float TickOffset;
	/** Height in of major ticks */
	float MajorTickHeight;
	/** Start layer for elements */
	int32 StartLayer;
	/** Draw effects to apply */
	ESlateDrawEffect::Type DrawEffects;
	/** Whether or not to only draw major ticks */
	bool bOnlyDrawMajorTicks;
	/** Whether or not to mirror labels */
	bool bMirrorLabels;
	
};

void FSequencerTimeSliderController::DrawTicks( FSlateWindowElementList& OutDrawElements, const struct FScrubRangeToScreen& RangeToScreen, FDrawTickArgs& InArgs ) const
{
	float MinDisplayTickSpacing = ScrubConstants::MinDisplayTickSpacing;
	if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSliderArgs.Settings->GetTimeSnapInterval()) && TimeSliderArgs.Settings->GetShowFrameNumbers())
	{
		MinDisplayTickSpacing = TimeSliderArgs.Settings->GetTimeSnapInterval();
	}

	const float Spacing = DetermineOptimalSpacing( RangeToScreen.PixelsPerInput, ScrubConstants::MinPixelsPerDisplayTick, MinDisplayTickSpacing );

	// Sub divisions
	// @todo Sequencer may need more robust calculation
	const int32 Divider = 10;
	// For slightly larger halfway tick mark
	const int32 HalfDivider = Divider / 2;
	// Find out where to start from
	int32 OffsetNum = FMath::FloorToInt(RangeToScreen.ViewInput.GetLowerBoundValue() / Spacing);
	
	FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8 );

	TArray<FVector2D> LinePoints;
	LinePoints.AddUninitialized(2);

	float Seconds = 0;
	while( (Seconds = OffsetNum*Spacing) < RangeToScreen.ViewInput.GetUpperBoundValue() )
	{
		// X position local to start of the widget area
		float XPos = RangeToScreen.InputToLocalX( Seconds );
		uint32 AbsOffsetNum = FMath::Abs(OffsetNum);

		if ( AbsOffsetNum % Divider == 0 )
		{
			FVector2D Offset( XPos, InArgs.TickOffset );
			FVector2D TickSize( 0.0f, InArgs.MajorTickHeight );

			LinePoints[0] = FVector2D( 0.0f,1.0f);
			LinePoints[1] = TickSize;

			// lines should not need anti-aliasing
			const bool bAntiAliasLines = false;

			// Draw each tick mark
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				InArgs.StartLayer,
				InArgs.AllottedGeometry.ToPaintGeometry( Offset, TickSize ),
				LinePoints,
				InArgs.ClippingRect,
				InArgs.DrawEffects,
				InArgs.TickColor,
				bAntiAliasLines
				);

			if( !InArgs.bOnlyDrawMajorTicks )
			{
				FString FrameString;
				if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSliderArgs.Settings->GetTimeSnapInterval()) && TimeSliderArgs.Settings->GetShowFrameNumbers())
				{
					FrameString = FString::Printf( TEXT("%d"), TimeToFrame(Seconds));
				}
				else
				{
					FrameString = Spacing == ScrubConstants::MinDisplayTickSpacing ? FString::Printf( TEXT("%.3f"), Seconds ) : FString::Printf( TEXT("%.2f"), Seconds );
				}

				// Space the text between the tick mark but slightly above
				const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);
				FVector2D TextOffset( XPos + 5.f, InArgs.bMirrorLabels ? 3.f : FMath::Abs( InArgs.AllottedGeometry.Size.Y - (InArgs.MajorTickHeight+3.f) ) );
				FSlateDrawElement::MakeText(
					OutDrawElements,
					InArgs.StartLayer+1, 
					InArgs.AllottedGeometry.ToPaintGeometry( TextOffset, TextSize ), 
					FrameString, 
					SmallLayoutFont, 
					InArgs.ClippingRect, 
					InArgs.DrawEffects,
					InArgs.TickColor*0.65f 
				);
			}
		}
		else if( !InArgs.bOnlyDrawMajorTicks )
		{
			// Compute the size of each tick mark.  If we are half way between to visible values display a slightly larger tick mark
			const float MinorTickHeight = AbsOffsetNum % HalfDivider == 0 ? 6.0f : 2.0f;

			FVector2D Offset(XPos, InArgs.bMirrorLabels ? 0.0f : FMath::Abs( InArgs.AllottedGeometry.Size.Y - MinorTickHeight ) );
			FVector2D TickSize(0.0f, MinorTickHeight);

			LinePoints[0] = FVector2D(0.0f,1.0f);
			LinePoints[1] = TickSize;

			const bool bAntiAlias = false;
			// Draw each sub mark
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				InArgs.StartLayer,
				InArgs.AllottedGeometry.ToPaintGeometry( Offset, TickSize ),
				LinePoints,
				InArgs.ClippingRect,
				InArgs.DrawEffects,
				InArgs.TickColor,
				bAntiAlias
			);
		}
		// Advance to next tick mark
		++OffsetNum;
	}
}


int32 FSequencerTimeSliderController::OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const bool bEnabled = bParentEnabled;
	const ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
	const float LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const float LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	const float LocalSequenceLength = LocalViewRangeMax-LocalViewRangeMin;
	
	FVector2D Scale = FVector2D(1.0f,1.0f);
	if ( LocalSequenceLength > 0)
	{
		FScrubRangeToScreen RangeToScreen( LocalViewRange, AllottedGeometry.Size );

		// draw tick marks
		const float MajorTickHeight = 9.0f;
	
		FDrawTickArgs Args;
		{
			Args.AllottedGeometry = AllottedGeometry;
			Args.bMirrorLabels = bMirrorLabels;
			Args.bOnlyDrawMajorTicks = false;
			Args.TickColor = FLinearColor::White;
			Args.ClippingRect = MyClippingRect;
			Args.DrawEffects = DrawEffects;
			Args.StartLayer = LayerId;
			Args.TickOffset = bMirrorLabels ? 0.0f : FMath::Abs( AllottedGeometry.Size.Y - MajorTickHeight );
			Args.MajorTickHeight = MajorTickHeight;
		}

		DrawTicks( OutDrawElements, RangeToScreen, Args );

		// draw playback & in/out range
		FPaintPlaybackRangeArgs PlaybackRangeArgs(
			bMirrorLabels ? FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_L") : FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_L"),
			bMirrorLabels ? FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_R") : FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_R"),
			6.f
		);

		LayerId = DrawPlaybackRange(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);
		LayerId = DrawInOutRange(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);

		float HalfSize = FMath::CeilToFloat(ScrubHandleSize / 2.0f);

		// Draw the scrub handle
		float XPos = RangeToScreen.InputToLocalX( TimeSliderArgs.ScrubPosition.Get() );
		const int32 ArrowLayer = LayerId + 2;
		FPaintGeometry MyGeometry =	AllottedGeometry.ToPaintGeometry( FVector2D( XPos-HalfSize, 0 ), FVector2D( ScrubHandleSize, AllottedGeometry.Size.Y ) );
		FLinearColor ScrubColor = InWidgetStyle.GetColorAndOpacityTint();
		{
			// @todo Sequencer this color should be specified in the style
			ScrubColor.A = ScrubColor.A * 0.75f;
			ScrubColor.B *= 0.1f;
			ScrubColor.G *= 0.2f;
		}

		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			ArrowLayer, 
			MyGeometry,
			bMirrorLabels ? ScrubHandleUp : ScrubHandleDown,
			MyClippingRect, 
			DrawEffects, 
			ScrubColor
		);

		// Draw the current time next to the scrub handle
		float Time = TimeSliderArgs.ScrubPosition.Get();
		FString FrameString;

		if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSliderArgs.Settings->GetTimeSnapInterval()) && TimeSliderArgs.Settings->GetShowFrameNumbers())
		{
			float FrameRate = 1.0f/TimeSliderArgs.Settings->GetTimeSnapInterval();
			float FrameTime = Time * FrameRate;
			int32 Frame = SequencerHelpers::TimeToFrame(Time, FrameRate);
			const float FrameTolerance = 0.001f;

			if (FMath::IsNearlyEqual(FrameTime, (float)Frame, FrameTolerance) || TimeSliderArgs.PlaybackStatus.Get() == EMovieScenePlayerStatus::Playing)
			{
				FrameString = FString::Printf( TEXT("%d"), TimeToFrame(Time));
			}
			else
			{
				FrameString = FString::Printf( TEXT("%.3f"), FrameTime);
			}
		}
		else
		{
			FrameString = FString::Printf( TEXT("%.2f"), Time );
		}

		FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );

		const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

		// Flip the text position if getting near the end of the view range
		if ((AllottedGeometry.Size.X - XPos) < (TextSize.X + 14.f))
		{
			XPos = XPos - TextSize.X - 12.f;
		}
		else
		{
			XPos = XPos + 10.f;
		}

		FVector2D TextOffset( XPos,  Args.bMirrorLabels ? TextSize.Y-6.f : Args.AllottedGeometry.Size.Y - (Args.MajorTickHeight+TextSize.Y) );

		FSlateDrawElement::MakeText(
			OutDrawElements,
			Args.StartLayer+1, 
			Args.AllottedGeometry.ToPaintGeometry( TextOffset, TextSize ), 
			FrameString, 
			SmallLayoutFont, 
			Args.ClippingRect, 
			Args.DrawEffects,
			Args.TickColor 
		);
		
		if (MouseDragType == DRAG_SETTING_RANGE)
		{
			float MouseStartPosX = RangeToScreen.InputToLocalX(MouseDownRange[0]);
			float MouseEndPosX = RangeToScreen.InputToLocalX(MouseDownRange[1]);

			float RangePosX = MouseStartPosX < MouseEndPosX ? MouseStartPosX : MouseEndPosX;
			float RangeSizeX = FMath::Abs(MouseStartPosX - MouseEndPosX);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry( FVector2D(RangePosX, 0.f), FVector2D(RangeSizeX, AllottedGeometry.Size.Y) ),
				bMirrorLabels ? ScrubHandleDown : ScrubHandleUp,
				MyClippingRect,
				DrawEffects,
				MouseStartPosX < MouseEndPosX ? FLinearColor(0.5f, 0.5f, 0.5f) : FLinearColor(0.25f, 0.3f, 0.3f)
			);
		}

		return ArrowLayer;
	}

	return LayerId;
}


int32 FSequencerTimeSliderController::DrawInOutRange(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	const TRange<float> InOutRange = TimeSliderArgs.InOutRange.Get();

	if (!InOutRange.IsEmpty())
	{
		const float InOutRangeL = RangeToScreen.InputToLocalX(InOutRange.GetLowerBoundValue()) - 1;
		const float InOutRangeR = RangeToScreen.InputToLocalX(InOutRange.GetUpperBoundValue()) + 1;
		const auto DrawColor = FLinearColor(FColor(32, 32, 128));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(InOutRangeL, 0.f), FVector2D(InOutRangeR - InOutRangeL, AllottedGeometry.Size.Y)),
			FEditorStyle::GetBrush("WhiteBrush"),
			MyClippingRect,
			ESlateDrawEffect::None,
			DrawColor.CopyWithNewOpacity(0.2f)
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(InOutRangeL, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
			Args.StartBrush,
			MyClippingRect,
			ESlateDrawEffect::None,
			DrawColor
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(InOutRangeR - Args.BrushWidth, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
			Args.EndBrush,
			MyClippingRect,
			ESlateDrawEffect::None,
			DrawColor
		);
	}

	return LayerId + 1;
}


int32 FSequencerTimeSliderController::DrawPlaybackRange(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	if (!TimeSliderArgs.PlaybackRange.IsSet())
	{
		return LayerId;
	}

	const TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	const float PlaybackRangeL = RangeToScreen.InputToLocalX(PlaybackRange.GetLowerBoundValue()) - 1;
	const float PlaybackRangeR = RangeToScreen.InputToLocalX(PlaybackRange.GetUpperBoundValue()) + 1;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeL, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		Args.StartBrush,
		MyClippingRect,
		ESlateDrawEffect::None,
		FColor(32, 128, 32)	// 120, 75, 50 (HSV)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeR - Args.BrushWidth, 0.f), FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y)),
		Args.EndBrush,
		MyClippingRect,
		ESlateDrawEffect::None,
		FColor(128, 32, 32)	// 0, 75, 50 (HSV)
	);

	// Black tint for excluded regions
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(0.f, 0.f), FVector2D(PlaybackRangeL, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		MyClippingRect,
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeR, 0.f), FVector2D(AllottedGeometry.Size.X - PlaybackRangeR, AllottedGeometry.Size.Y)),
		FEditorStyle::GetBrush("WhiteBrush"),
		MyClippingRect,
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f)
	);

	return LayerId + 1;
}

FReply FSequencerTimeSliderController::OnMouseButtonDown( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bHandleLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	bool bHandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && TimeSliderArgs.AllowZoom;
	
	DistanceDragged = 0;

	FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
	MouseDownRange[0] = RangeToScreen.LocalXToInput(MyGeometry.AbsoluteToLocal(MouseEvent.GetLastScreenSpacePosition()).X);
	MouseDownRange[1] = MouseDownRange[0];

	if ( bHandleLeftMouseButton )
	{
		return FReply::Handled().CaptureMouse( WidgetOwner.AsShared() ).PreventThrottling();
	}
	else if ( bHandleRightMouseButton )
	{
		// Always capture mouse if we left or right click on the widget
		return FReply::Handled().CaptureMouse( WidgetOwner.AsShared() );
	}

	return FReply::Unhandled();
}

FReply FSequencerTimeSliderController::OnMouseButtonUp( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bHandleLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && WidgetOwner.HasMouseCapture();
	bool bHandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && WidgetOwner.HasMouseCapture() && TimeSliderArgs.AllowZoom ;

	if ( bHandleRightMouseButton )
	{
		if (!bPanning)
		{
			// Open a context menu if allowed
			if (ContextMenuSuppression == 0 && TimeSliderArgs.PlaybackRange.IsSet())
			{
				FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
				FVector2D CursorPos = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );

				float MouseValue = RangeToScreen.LocalXToInput( CursorPos.X );
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					MouseValue = TimeSliderArgs.Settings->SnapTimeToInterval(MouseValue);
				}

				TSharedRef<SWidget> MenuContent = OpenSetPlaybackRangeMenu(MouseValue);
				FSlateApplication::Get().PushMenu(
					WidgetOwner.AsShared(),
					MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath(),
					MenuContent,
					MouseEvent.GetScreenSpacePosition(),
					FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
					);

				return FReply::Handled().SetUserFocus(MenuContent, EFocusCause::SetDirectly).ReleaseMouseCapture();
			}

			// return unhandled in case our parent wants to use our right mouse button to open a context menu
			return FReply::Unhandled().ReleaseMouseCapture();
		}
		
		bPanning = false;
		
		return FReply::Handled().ReleaseMouseCapture();
	}
	else if ( bHandleLeftMouseButton )
	{
		if (MouseDragType == DRAG_START_RANGE)
		{
			TimeSliderArgs.OnEndPlaybackRangeDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_END_RANGE)
		{
			TimeSliderArgs.OnEndPlaybackRangeDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_IN_RANGE)
		{
			TimeSliderArgs.OnEndInOutRangeDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_OUT_RANGE)
		{
			TimeSliderArgs.OnEndInOutRangeDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SETTING_RANGE)
		{
			FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
			FVector2D CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetLastScreenSpacePosition());
			float NewValue = RangeToScreen.LocalXToInput(CursorPos.X);

			if ( TimeSliderArgs.Settings->GetIsSnapEnabled() )
			{
				NewValue = TimeSliderArgs.Settings->SnapTimeToInterval(NewValue);
			}

			float DownValue = MouseDownRange[0];
				
			if ( TimeSliderArgs.Settings->GetIsSnapEnabled() )
			{
				DownValue = TimeSliderArgs.Settings->SnapTimeToInterval(DownValue);
			}

			// Zoom in
			bool bDoSetRange = false;
			if (NewValue > DownValue)
			{
				// push the current value onto the stack
				RangeStack.Add(FVector2D(TimeSliderArgs.ViewRange.Get().GetLowerBoundValue(), TimeSliderArgs.ViewRange.Get().GetUpperBoundValue()));
				bDoSetRange = true;
			}
			// Zoom out
			else if (RangeStack.Num())
			{
				// pop the stack
				FVector2D LastRange = RangeStack.Pop();
				DownValue = LastRange[0];
				NewValue = LastRange[1];
				bDoSetRange = true;
			}

			if (bDoSetRange)
			{
				TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound(TRange<float>(DownValue, NewValue), EViewRangeInterpolation::Immediate);
					
				if( !TimeSliderArgs.ViewRange.IsBound() )
				{	
					// The output is not bound to a delegate so we'll manage the value ourselves
					TimeSliderArgs.ViewRange.Set( TRange<float>( DownValue, NewValue ) );
				}
			}
		}
		else
		{
			TimeSliderArgs.OnEndScrubberMovement.ExecuteIfBound();

			FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
			FVector2D CursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetLastScreenSpacePosition());
			float NewValue = RangeToScreen.LocalXToInput(CursorPos.X);

			if ( TimeSliderArgs.Settings->GetIsSnapEnabled() && TimeSliderArgs.Settings->GetSnapPlayTimeToInterval() )
			{
				NewValue = TimeSliderArgs.Settings->SnapTimeToInterval(NewValue);
			}
			
			CommitScrubPosition( NewValue, /*bIsScrubbing=*/false );
		}

		MouseDragType = DRAG_NONE;
		return FReply::Handled().ReleaseMouseCapture();

	}

	return FReply::Unhandled();
}


FReply FSequencerTimeSliderController::OnMouseMove( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (!WidgetOwner.HasMouseCapture())
	{
		return FReply::Unhandled();
	}

	if (MouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ))
	{
		if (!bPanning)
		{
			DistanceDragged += FMath::Abs( MouseEvent.GetCursorDelta().X );
			if ( DistanceDragged > FSlateApplication::Get().GetDragTriggerDistance() )
			{
				bPanning = true;
			}
		}
		else
		{
			TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
			float LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
			float LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();

			FScrubRangeToScreen ScaleInfo( LocalViewRange, MyGeometry.Size );
			FVector2D ScreenDelta = MouseEvent.GetCursorDelta();
			FVector2D InputDelta;
			InputDelta.X = ScreenDelta.X/ScaleInfo.PixelsPerInput;

			float NewViewOutputMin = LocalViewRangeMin - InputDelta.X;
			float NewViewOutputMax = LocalViewRangeMax - InputDelta.X;

			ClampViewRange(NewViewOutputMin, NewViewOutputMax);
			SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Immediate);
		}
	}
	else if (MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ))
	{
		TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
		DistanceDragged += FMath::Abs( MouseEvent.GetCursorDelta().X );

		if ( MouseDragType == DRAG_NONE )
		{
			if ( DistanceDragged > FSlateApplication::Get().GetDragTriggerDistance() )
			{
				FScrubRangeToScreen RangeToScreen(LocalViewRange, MyGeometry.Size);
				const float ScrubPosition = TimeSliderArgs.ScrubPosition.Get();

				TRange<float> InOutRange = TimeSliderArgs.InOutRange.Get();
				TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
				float LocalMouseDownPos = RangeToScreen.InputToLocalX(MouseDownRange[0]);

				if (HitTestScrubberEnd(RangeToScreen, InOutRange, LocalMouseDownPos, ScrubPosition))
				{
					// in/out range end scrubber
					MouseDragType = DRAG_OUT_RANGE;
					TimeSliderArgs.OnBeginInOutRangeDrag.ExecuteIfBound();
				}
				else if (HitTestScrubberStart(RangeToScreen, InOutRange, LocalMouseDownPos, ScrubPosition))
				{
					// in/out range start scrubber
					MouseDragType = DRAG_IN_RANGE;
					TimeSliderArgs.OnBeginInOutRangeDrag.ExecuteIfBound();
				}
				else if (HitTestScrubberEnd(RangeToScreen, PlaybackRange, LocalMouseDownPos, ScrubPosition))
				{
					// playback range end scrubber
					MouseDragType = DRAG_END_RANGE;
					TimeSliderArgs.OnBeginPlaybackRangeDrag.ExecuteIfBound();
				}
				else if (HitTestScrubberStart(RangeToScreen, PlaybackRange, LocalMouseDownPos, ScrubPosition))
				{
					// playback range start scrubber
					MouseDragType = DRAG_START_RANGE;
					TimeSliderArgs.OnBeginPlaybackRangeDrag.ExecuteIfBound();
				}
				else if (FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Control))
				{
					MouseDragType = DRAG_SETTING_RANGE;
				}
				else
				{
					MouseDragType = DRAG_SCRUBBING_TIME;
					TimeSliderArgs.OnBeginScrubberMovement.ExecuteIfBound();
				}
			}
		}
		else
		{
			FScrubRangeToScreen RangeToScreen( TimeSliderArgs.ViewRange.Get(), MyGeometry.Size );
			FVector2D CursorPos = MyGeometry.AbsoluteToLocal( MouseEvent.GetLastScreenSpacePosition() );
			float NewValue = RangeToScreen.LocalXToInput( CursorPos.X );


			// Set the start range time?
			if (MouseDragType == DRAG_START_RANGE)
			{
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					NewValue = TimeSliderArgs.Settings->SnapTimeToInterval(NewValue);
				}

				SetPlaybackRangeStart(NewValue);
			}
			// Set the end range time?
			else if(MouseDragType == DRAG_END_RANGE)
			{
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					NewValue = TimeSliderArgs.Settings->SnapTimeToInterval(NewValue);
				}
					
				SetPlaybackRangeEnd(NewValue);
			}
			else if (MouseDragType == DRAG_IN_RANGE)
			{
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					NewValue = TimeSliderArgs.Settings->SnapTimeToInterval(NewValue);
				}

				TRange<float> InOutRange = TimeSliderArgs.InOutRange.Get();

				if (NewValue <= InOutRange.GetUpperBoundValue())
				{
					TimeSliderArgs.OnInOutRangeChanged.ExecuteIfBound(TRange<float>(NewValue, InOutRange.GetUpperBoundValue()));
				}
			}
			// Set the end range time?
			else if(MouseDragType == DRAG_OUT_RANGE)
			{
				if (TimeSliderArgs.Settings->GetIsSnapEnabled())
				{
					NewValue = TimeSliderArgs.Settings->SnapTimeToInterval(NewValue);
				}
					
				TRange<float> InOutRange = TimeSliderArgs.InOutRange.Get();

				if (NewValue >= InOutRange.GetLowerBoundValue())
				{
					TimeSliderArgs.OnInOutRangeChanged.ExecuteIfBound(TRange<float>(InOutRange.GetLowerBoundValue(), NewValue));
				}
			}
			else if (MouseDragType == DRAG_SCRUBBING_TIME)
			{
				if ( TimeSliderArgs.Settings->GetIsSnapEnabled() && TimeSliderArgs.Settings->GetSnapPlayTimeToInterval() )
				{
					NewValue = TimeSliderArgs.Settings->SnapTimeToInterval(NewValue);
				}
					
				// Delegate responsibility for clamping to the current viewrange to the client
				CommitScrubPosition( NewValue, /*bIsScrubbing=*/true );
			}
			else if (MouseDragType == DRAG_SETTING_RANGE)
			{
				MouseDownRange[1] = NewValue;
			}
		}
	}

	return FReply::Handled();
}


void FSequencerTimeSliderController::CommitScrubPosition( float NewValue, bool bIsScrubbing )
{
	// Manage the scrub position ourselves if its not bound to a delegate
	if ( !TimeSliderArgs.ScrubPosition.IsBound() )
	{
		TimeSliderArgs.ScrubPosition.Set( NewValue );
	}

	TimeSliderArgs.OnScrubPositionChanged.ExecuteIfBound( NewValue, bIsScrubbing );
}

FReply FSequencerTimeSliderController::OnMouseWheel( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	TOptional<TRange<float>> NewTargetRange;

	float MouseFractionX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / MyGeometry.GetLocalSize().X;
	if ( TimeSliderArgs.AllowZoom && MouseEvent.IsControlDown() )
	{
		const float ZoomDelta = -0.2f * MouseEvent.GetWheelDelta();
		if (ZoomByDelta(ZoomDelta, MouseFractionX))
		{
			return FReply::Handled();
		}
	}
	else if (MouseEvent.IsShiftDown())
	{
		PanByDelta(-MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FCursorReply FSequencerTimeSliderController::OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	FScrubRangeToScreen RangeToScreen(TimeSliderArgs.ViewRange.Get(), MyGeometry.Size);
	TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	TRange<float> InOutRange = TimeSliderArgs.InOutRange.Get();
	const float ScrubPosition = TimeSliderArgs.ScrubPosition.Get();

	// Use L/R resize cursor if we're dragging or hovering a playback range bound
	float HitTestPosition = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition()).X;
	if ((MouseDragType == DRAG_END_RANGE) ||
		(MouseDragType == DRAG_START_RANGE) ||
		(MouseDragType == DRAG_IN_RANGE) ||
		(MouseDragType == DRAG_OUT_RANGE) ||
		HitTestScrubberStart(RangeToScreen, PlaybackRange, HitTestPosition, ScrubPosition) ||
		HitTestScrubberEnd(RangeToScreen, PlaybackRange, HitTestPosition, ScrubPosition) ||
		HitTestScrubberStart(RangeToScreen, InOutRange, HitTestPosition, ScrubPosition) ||
		HitTestScrubberEnd(RangeToScreen, InOutRange, HitTestPosition, ScrubPosition))
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	return FCursorReply::Unhandled();
}

int32 FSequencerTimeSliderController::OnPaintSectionView( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintSectionAreaViewArgs& Args ) const
{
	const ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get();
	float LocalScrubPosition = TimeSliderArgs.ScrubPosition.Get();

	float ViewRange = LocalViewRange.Size<float>();
	float PixelsPerInput = ViewRange > 0 ? AllottedGeometry.Size.X / ViewRange : 0;
	float LinePos =  (LocalScrubPosition - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;

	FScrubRangeToScreen RangeToScreen( LocalViewRange, AllottedGeometry.Size );

	if (Args.PlaybackRangeArgs.IsSet())
	{
		LayerId = DrawPlaybackRange(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, RangeToScreen, Args.PlaybackRangeArgs.GetValue());
		LayerId = DrawInOutRange(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, RangeToScreen, Args.PlaybackRangeArgs.GetValue());
	}

	if( Args.bDisplayTickLines )
	{
		static FLinearColor TickColor(0.f, 0.f, 0.f, 0.3f);
		
		// Draw major tick lines in the section area
		FDrawTickArgs DrawTickArgs;
		{
			DrawTickArgs.AllottedGeometry = AllottedGeometry;
			DrawTickArgs.bMirrorLabels = false;
			DrawTickArgs.bOnlyDrawMajorTicks = true;
			DrawTickArgs.TickColor = TickColor;
			DrawTickArgs.ClippingRect = MyClippingRect;
			DrawTickArgs.DrawEffects = DrawEffects;
			// Draw major ticks under sections
			DrawTickArgs.StartLayer = LayerId-1;
			// Draw the tick the entire height of the section area
			DrawTickArgs.TickOffset = 0.0f;
			DrawTickArgs.MajorTickHeight = AllottedGeometry.Size.Y;
		}

		DrawTicks( OutDrawElements, RangeToScreen, DrawTickArgs );
	}

	if( Args.bDisplayScrubPosition )
	{
		// Draw a line for the scrub position
		TArray<FVector2D> LinePoints;
		{
			LinePoints.AddUninitialized(2);
			LinePoints[0] = FVector2D( 0.0f, 0.0f );
			LinePoints[1] = FVector2D( 0.0f, FMath::RoundToFloat( AllottedGeometry.Size.Y ) );
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId+1,
			AllottedGeometry.ToPaintGeometry( FVector2D(LinePos, 0.0f ), FVector2D(1.0f,1.0f) ),
			LinePoints,
			MyClippingRect,
			DrawEffects,
			FLinearColor::White,
			false
		);
	}

	return LayerId;
}

TSharedRef<SWidget> FSequencerTimeSliderController::OpenSetPlaybackRangeMenu(float MouseTime)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	FText NumericText;
	if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSliderArgs.Settings->GetTimeSnapInterval()) && TimeSliderArgs.Settings->GetShowFrameNumbers())
	{
		NumericText = FText::Format(LOCTEXT("FrameTextFormat", "Playback Range (at frame {0}):"), FText::AsNumber(TimeToFrame(MouseTime)));
	}
	else
	{
		NumericText = FText::Format(LOCTEXT("TimeTextFormat", "Playback Range (at {0}s):"), FText::AsNumber(MouseTime));
	}

	TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	MenuBuilder.BeginSection("SequencerPlaybackRangeMenu", NumericText);
	{
		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("SetPlaybackStart", "Set Start Time"), NumericText),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ return SetPlaybackRangeStart(MouseTime); }),
				FCanExecuteAction::CreateLambda([=]{ return MouseTime <= PlaybackRange.GetUpperBoundValue(); })
			)
		);

		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("SetPlaybackEnd", "Set End Time"), NumericText),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ return SetPlaybackRangeEnd(MouseTime); }),
				FCanExecuteAction::CreateLambda([=]{ return MouseTime >= PlaybackRange.GetLowerBoundValue(); })
			)
		);
	}
	MenuBuilder.EndSection(); // SequencerPlaybackRangeMenu

	return MenuBuilder.MakeWidget();
}

int32 FSequencerTimeSliderController::TimeToFrame(float Time) const
{
	float FrameRate = 1.0f/TimeSliderArgs.Settings->GetTimeSnapInterval();
	return SequencerHelpers::TimeToFrame(Time, FrameRate);
}

float FSequencerTimeSliderController::FrameToTime(int32 Frame) const
{
	float FrameRate = 1.0f/TimeSliderArgs.Settings->GetTimeSnapInterval();
	return SequencerHelpers::FrameToTime(Frame, FrameRate);
}

void FSequencerTimeSliderController::ClampViewRange(float& NewRangeMin, float& NewRangeMax)
{
	bool bNeedsClampSet = false;
	float NewClampRangeMin = TimeSliderArgs.ClampRange.Get().GetLowerBoundValue();
	if ( NewRangeMin < TimeSliderArgs.ClampRange.Get().GetLowerBoundValue() )
	{
		NewClampRangeMin = NewRangeMin;
		bNeedsClampSet = true;
	}

	float NewClampRangeMax = TimeSliderArgs.ClampRange.Get().GetUpperBoundValue();
	if ( NewRangeMax > TimeSliderArgs.ClampRange.Get().GetUpperBoundValue() )
	{
		NewClampRangeMax = NewRangeMax;
		bNeedsClampSet = true;
	}

	if (bNeedsClampSet)
	{
		SetClampRange(NewClampRangeMin, NewClampRangeMax);
	}
}

void FSequencerTimeSliderController::SetViewRange( float NewRangeMin, float NewRangeMax, EViewRangeInterpolation Interpolation )
{
	const TRange<float> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound( NewRange, Interpolation );

	if( !TimeSliderArgs.ViewRange.IsBound() )
	{	
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ViewRange.Set( NewRange );
	}
}

void FSequencerTimeSliderController::SetClampRange( float NewRangeMin, float NewRangeMax )
{
	const TRange<float> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnClampRangeChanged.ExecuteIfBound(NewRange);

	if( !TimeSliderArgs.ClampRange.IsBound() )
	{	
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ClampRange.Set(NewRange);
	}
}

void FSequencerTimeSliderController::SetPlayRange( float NewRangeMin, float NewRangeMax )
{
	const TRange<float> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(NewRange);

	if( !TimeSliderArgs.PlaybackRange.IsBound() )
	{	
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.PlaybackRange.Set(NewRange);
	}
}

bool FSequencerTimeSliderController::ZoomByDelta( float InDelta, float MousePositionFraction )
{
	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get().GetAnimationTarget();
	float LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	float LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const float OutputViewSize = LocalViewRangeMax - LocalViewRangeMin;
	const float OutputChange = OutputViewSize * InDelta;

	float NewViewOutputMin = LocalViewRangeMin - (OutputChange * MousePositionFraction);
	float NewViewOutputMax = LocalViewRangeMax + (OutputChange * (1.f - MousePositionFraction));

	if( NewViewOutputMin < NewViewOutputMax )
	{
		ClampViewRange(NewViewOutputMin, NewViewOutputMax);
		SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
		return true;
	}

	return false;
}

void FSequencerTimeSliderController::PanByDelta( float InDelta )
{
	TRange<float> LocalViewRange = TimeSliderArgs.ViewRange.Get().GetAnimationTarget();

	float CurrentMin = LocalViewRange.GetLowerBoundValue();
	float CurrentMax = LocalViewRange.GetUpperBoundValue();

	// Adjust the delta to be a percentage of the current range
	InDelta *= ScrubConstants::ScrollPanFraction * (CurrentMax - CurrentMin);

	float NewViewOutputMin = CurrentMin + InDelta;
	float NewViewOutputMax = CurrentMax + InDelta;

	ClampViewRange(NewViewOutputMin, NewViewOutputMax);
	SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
}


bool FSequencerTimeSliderController::HitTestScrubberStart(const FScrubRangeToScreen& RangeToScreen, const TRange<float>& PlaybackRange, float LocalHitPositionX, float ScrubPosition) const
{
	static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f;
	float LocalPlaybackStartPos = RangeToScreen.InputToLocalX(PlaybackRange.GetLowerBoundValue());

	// We favor hit testing the scrub bar over hit testing the playback range bounds
	if (FMath::IsNearlyEqual(LocalPlaybackStartPos, RangeToScreen.InputToLocalX(ScrubPosition), ScrubHandleSize/2.f))
	{
		return false;
	}

	// Hit test against the brush region to the right of the playback start position, +/- DragToleranceSlateUnits
	return LocalHitPositionX >= LocalPlaybackStartPos - DragToleranceSlateUnits &&
		LocalHitPositionX <= LocalPlaybackStartPos + BrushSizeInStateUnits + DragToleranceSlateUnits;
}

bool FSequencerTimeSliderController::HitTestScrubberEnd(const FScrubRangeToScreen& RangeToScreen, const TRange<float>& PlaybackRange, float LocalHitPositionX, float ScrubPosition) const
{
	static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f;
	float LocalPlaybackEndPos = RangeToScreen.InputToLocalX(PlaybackRange.GetUpperBoundValue());
	
	// We favor hit testing the scrub bar over hit testing the playback range bounds
	if (FMath::IsNearlyEqual(LocalPlaybackEndPos, RangeToScreen.InputToLocalX(ScrubPosition), ScrubHandleSize/2.f))
	{
		return false;
	}
	
	// Hit test against the brush region to the left of the playback end position, +/- DragToleranceSlateUnits
	return LocalHitPositionX >= LocalPlaybackEndPos - BrushSizeInStateUnits - DragToleranceSlateUnits &&
		LocalHitPositionX <= LocalPlaybackEndPos + DragToleranceSlateUnits;
}

void FSequencerTimeSliderController::SetPlaybackRangeStart(float NewStart)
{
	TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewStart <= PlaybackRange.GetUpperBoundValue())
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<float>(NewStart, PlaybackRange.GetUpperBoundValue()));
	}
}

void FSequencerTimeSliderController::SetPlaybackRangeEnd(float NewEnd)
{
	TRange<float> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewEnd >= PlaybackRange.GetLowerBoundValue())
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<float>(PlaybackRange.GetLowerBoundValue(), NewEnd));
	}
}

#undef LOCTEXT_NAMESPACE
