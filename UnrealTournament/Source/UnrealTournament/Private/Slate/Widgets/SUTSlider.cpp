// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTSlider.h"

#if !UE_SERVER

void SUTSlider::Construct( const FArguments& InArgs )
{
	SnapCount = InArgs._SnapCount;
	SSlider::Construct( SSlider::FArguments()
		.IndentHandle(InArgs._IndentHandle)
		.Locked(InArgs._Locked)
		.Orientation(InArgs._Orientation)
		.SliderBarColor(InArgs._SliderBarColor)
		.SliderHandleColor(InArgs._SliderHandleColor)
		.Style(InArgs._Style)
		.StepSize(InArgs._StepSize)
		.Value(InArgs._Value)
		.IsFocusable(InArgs._IsFocusable)
		.OnMouseCaptureBegin(InArgs._OnMouseCaptureBegin)
		.OnMouseCaptureEnd(InArgs._OnMouseCaptureEnd)
		.OnControllerCaptureBegin(InArgs._OnControllerCaptureBegin)
		.OnControllerCaptureEnd(InArgs._OnControllerCaptureEnd)
		.OnValueChanged(InArgs._OnValueChanged));

	if (SnapCount.Get() > 0)
	{
		SnapTo(InArgs._InitialSnap);
	}
}

void SUTSlider::CommitValue(float NewValue)
{
	if (SnapCount.Get() > 1)
	{
		float OldValue = NewValue;
		float fSnapCount = float(SnapCount.Get()-1);
		float fHalfPoint = 0.5f / fSnapCount;
		NewValue = FPlatformMath::FloorToInt(fSnapCount * (NewValue + fHalfPoint)) / fSnapCount;
	}

	SSlider::CommitValue(NewValue);
}

/** 
 * We have to duplicate the whole OnPaint function because we want the tick marks to appear behind the 
 **/
int32 SUTSlider::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// we draw the slider like a horizontal slider regardless of the orientation, and apply a render transform to make it display correctly.
	// However, the AllottedGeometry is computed as it will be rendered, so we have to use the "horizontal orientation" when doing drawing computations.
	const float AllottedWidth = Orientation == Orient_Horizontal ? AllottedGeometry.GetLocalSize().X : AllottedGeometry.GetLocalSize().Y;
	const float AllottedHeight = Orientation == Orient_Horizontal ? AllottedGeometry.GetLocalSize().Y : AllottedGeometry.GetLocalSize().X;

	float HandleRotation;
	FVector2D HandleTopLeftPoint;
	FVector2D SliderStartPoint;
	FVector2D SliderEndPoint;

	// calculate slider geometry as if it's a horizontal slider (we'll rotate it later if it's vertical)
	const FVector2D HandleSize = Style->NormalThumbImage.ImageSize;
	const FVector2D HalfHandleSize = 0.5f * HandleSize;
	const float Indentation = IndentHandle.Get() ? HandleSize.X : 0.0f;

	const float SliderLength = AllottedWidth - Indentation;
	const float SliderPercent = ValueAttribute.Get();
	const float SliderHandleOffset = SliderPercent * SliderLength;
	const float SliderY = 0.5f * AllottedHeight;

	HandleRotation = 0.0f;
	HandleTopLeftPoint = FVector2D(SliderHandleOffset - ( HandleSize.X * SliderPercent ) + 0.5f * Indentation, SliderY - HalfHandleSize.Y);

	SliderStartPoint = FVector2D(HalfHandleSize.X, SliderY);
	SliderEndPoint = FVector2D(AllottedWidth - HalfHandleSize.X, SliderY);

	FSlateRect RotatedClippingRect = MyClippingRect;
	FGeometry SliderGeometry = AllottedGeometry;
	
	// rotate the slider 90deg if it's vertical. The 0 side goes on the bottom, the 1 side on the top.
	if (Orientation == Orient_Vertical)
	{
		// Do this by translating along -X by the width of the geometry, then rotating 90 degreess CCW (left-hand coords)
		FSlateRenderTransform SlateRenderTransform = TransformCast<FSlateRenderTransform>(Concatenate(Inverse(FVector2D(AllottedWidth, 0)), FQuat2D(FMath::DegreesToRadians(-90.0f))));
		// create a child geometry matching this one, but with the render transform.
		SliderGeometry = AllottedGeometry.MakeChild(
			FVector2D(AllottedWidth, AllottedHeight), 
			FSlateLayoutTransform(), 
			SlateRenderTransform, FVector2D::ZeroVector);
		// The clipping rect is already given properly in window space. But we do not support layout rotations, so our local space rendering cannot
		// get the clipping rect into local space properly for the local space clipping we do in the shader.
		// Thus, we transform the clip coords into local space manually, UNDO the render transform so it will clip properly,
		// and then bring the clip coords back into window space where DrawElements expect them.
		RotatedClippingRect = TransformRect(
			Concatenate(
				Inverse(SliderGeometry.GetAccumulatedLayoutTransform()), 
				Inverse(SlateRenderTransform),
				SliderGeometry.GetAccumulatedLayoutTransform()), 
			MyClippingRect);
	}

	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	// draw slider bar
	auto BarTopLeft = FVector2D(SliderStartPoint.X, SliderStartPoint.Y - Style->BarThickness * 0.5f);
	auto BarSize = FVector2D(SliderEndPoint.X - SliderStartPoint.X, Style->BarThickness);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		SliderGeometry.ToPaintGeometry(BarTopLeft, BarSize),
		LockedAttribute.Get() ? &Style->DisabledBarImage : &Style->NormalBarImage,
		RotatedClippingRect,
		DrawEffects,
		SliderBarColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);

	if (SnapCount.Get() > 1)
	{
		++LayerId;

		float ActualBarWidth = 	SliderEndPoint.X - SliderStartPoint.X;
		float TickSpace = ActualBarWidth / float(SnapCount.Get()-1);
		FVector2D TickPosition = SliderStartPoint;

		for (int32 Tick = 0; Tick < SnapCount.Get(); Tick++)
		{
			// Draw the tick -- NOTE: this should be moved to a style
			FVector2D TickTopLeft = FVector2D(TickPosition.X - 1, TickPosition.Y - Style->BarThickness * 2);
			FVector2D TickSize = FVector2D(3, Style->BarThickness * 4.0f);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				SliderGeometry.ToPaintGeometry(TickTopLeft, TickSize),
				LockedAttribute.Get() ? &Style->DisabledBarImage : &Style->NormalBarImage,
				RotatedClippingRect,
				DrawEffects,
				SliderBarColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
				);

			TickPosition.X += TickSpace;
		}
	}

	++LayerId;

	// draw slider thumb
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId,
		SliderGeometry.ToPaintGeometry(HandleTopLeftPoint, Style->NormalThumbImage.ImageSize),
		LockedAttribute.Get() ? &Style->DisabledThumbImage : &Style->NormalThumbImage,
		RotatedClippingRect,
		DrawEffects,
		SliderHandleColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
	);

	return LayerId;
}

void SUTSlider::SnapTo(int32 SnapIndex)
{
	if (SnapCount.Get() > 0 && SnapIndex < SnapCount.Get())
	{
		SetValue(float(SnapIndex) / float(SnapCount.Get()-1));
	}
	else
	{
		SetValue(1.0f);
	}
}

int32 SUTSlider::GetSnapValue()
{
	return int32( float(SnapCount.Get()) * GetValue() );
}



#endif