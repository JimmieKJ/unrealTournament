// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

void SSlider::Construct( const SSlider::FArguments& InDeclaration )
{
	check(InDeclaration._Style);

	Style = InDeclaration._Style;

	IndentHandle = InDeclaration._IndentHandle;
	LockedAttribute = InDeclaration._Locked;
	Orientation = InDeclaration._Orientation;
	ValueAttribute = InDeclaration._Value;
	SliderBarColor = InDeclaration._SliderBarColor;
	SliderHandleColor = InDeclaration._SliderHandleColor;
	OnMouseCaptureBegin = InDeclaration._OnMouseCaptureBegin;
	OnMouseCaptureEnd = InDeclaration._OnMouseCaptureEnd;

	OnValueChanged = InDeclaration._OnValueChanged;
}

int32 SSlider::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
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
	const FVector2D HalfHandleSize = 0.5f * Style->NormalThumbImage.ImageSize;
	const float Indentation = IndentHandle.Get() ? Style->NormalThumbImage.ImageSize.X : 0.0f;

	const float SliderLength = AllottedWidth - Indentation;
	const float SliderHandleOffset = ValueAttribute.Get() * SliderLength;
	const float SliderY = 0.5f * AllottedHeight;

	HandleRotation = 0.0f;
	HandleTopLeftPoint = FVector2D(SliderHandleOffset - HalfHandleSize.X + 0.5f * Indentation, SliderY - HalfHandleSize.Y);

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
	auto BarTopLeft = FVector2D(SliderStartPoint.X, SliderStartPoint.Y - 1);
	auto BarSize = FVector2D(SliderEndPoint.X - SliderStartPoint.X, 2);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		SliderGeometry.ToPaintGeometry(BarTopLeft, BarSize),
		LockedAttribute.Get() ? &Style->DisabledBarImage : &Style->NormalBarImage,
		RotatedClippingRect,
		DrawEffects,
		SliderBarColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);

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

FVector2D SSlider::ComputeDesiredSize( float ) const
{
	static const FVector2D SSliderDesiredSize(16.0f, 16.0f);

	if ( Style == nullptr )
	{
		return SSliderDesiredSize;
	}

	if (Orientation == Orient_Vertical)
	{
		return FVector2D(Style->NormalThumbImage.ImageSize.Y, SSliderDesiredSize.Y);
	}

	return FVector2D(SSliderDesiredSize.X, Style->NormalThumbImage.ImageSize.Y);
}

FReply SSlider::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && !LockedAttribute.Get())
	{
		OnMouseCaptureBegin.ExecuteIfBound();
		CommitValue(PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition()));

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SSlider::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && HasMouseCapture())
	{
		SetCursor(EMouseCursor::Default);
		OnMouseCaptureEnd.ExecuteIfBound();

		return FReply::Handled().ReleaseMouseCapture();	
	}

	return FReply::Unhandled();
}

FReply SSlider::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (this->HasMouseCapture() && !LockedAttribute.Get())
	{
		SetCursor((Orientation == Orient_Horizontal) ? EMouseCursor::ResizeLeftRight : EMouseCursor::ResizeUpDown);
		CommitValue(PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition()));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSlider::CommitValue(float NewValue)
{
	if (!ValueAttribute.IsBound())
	{
		ValueAttribute.Set(NewValue);
	}

	OnValueChanged.ExecuteIfBound(NewValue);
}

float SSlider::PositionToValue( const FGeometry& MyGeometry, const FVector2D& AbsolutePosition )
{
	const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(AbsolutePosition);
	const float Indentation = IndentHandle.Get() ? Style->NormalThumbImage.ImageSize.X : 0.0f;

	float RelativeValue;

	if (Orientation == Orient_Horizontal)
	{
		RelativeValue = (LocalPosition.X - 0.5f * Indentation) / (MyGeometry.Size.X - Indentation);
	}
	else
	{
		RelativeValue = (MyGeometry.Size.Y - LocalPosition.Y - 0.5f * Indentation) / (MyGeometry.Size.Y - Indentation);
	}
	
	return FMath::Clamp(RelativeValue, 0.0f, 1.0f);
}

float SSlider::GetValue() const
{
	return ValueAttribute.Get();
}

void SSlider::SetValue(const TAttribute<float>& InValueAttribute)
{
	ValueAttribute = InValueAttribute;
}

void SSlider::SetIndentHandle(const TAttribute<bool>& InIndentHandle)
{
	IndentHandle = InIndentHandle;
}

void SSlider::SetLocked(const TAttribute<bool>& InLocked)
{
	LockedAttribute = InLocked;
}

void SSlider::SetOrientation(EOrientation InOrientation)
{
	Orientation = InOrientation;
}

void SSlider::SetSliderBarColor(FSlateColor InSliderBarColor)
{
	SliderBarColor = InSliderBarColor;
}

void SSlider::SetSliderHandleColor(FSlateColor InSliderHandleColor)
{
	SliderHandleColor = InSliderHandleColor;
}
