// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "SUTProgressSlider.h"

#if !UE_SERVER

void SUTProgressSlider::Construct(const SUTProgressSlider::FArguments& InDeclaration)
{
	bSliding = false;
	DeferedValue = 0.0f;

	MarkStart = -1.0f;
	MarkEnd = -1.0f;

	check(InDeclaration._Style);

	Style = InDeclaration._Style;

	IndentHandle = InDeclaration._IndentHandle;
	LockedAttribute = InDeclaration._Locked;
	Orientation = InDeclaration._Orientation;
	ValueAttribute = InDeclaration._Value;
	SliderBarColor = InDeclaration._SliderBarColor;
	SliderBarBGColor = InDeclaration._SliderBarBGColor;
	SliderHandleColor = InDeclaration._SliderHandleColor;
	OnMouseCaptureBegin = InDeclaration._OnMouseCaptureBegin;
	OnMouseCaptureEnd = InDeclaration._OnMouseCaptureEnd;
	OnValueChanged = InDeclaration._OnValueChanged;
	DeferValueAttribute = InDeclaration._DeferValue;
	TotalValueAttribute = InDeclaration._TotalValue;
}

int32 SUTProgressSlider::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
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
	const float SliderHandleOffset = (DeferValueAttribute.Get() && bSliding ? DeferedValue : ValueAttribute.Get()) * SliderLength;
	
	HandleRotation = 0.0f;
	HandleTopLeftPoint = FVector2D(SliderHandleOffset - HalfHandleSize.X + 0.5f * Indentation, 0.5f * AllottedHeight - HalfHandleSize.Y);

	const float BarHeight = 0.7f * AllottedHeight;
	SliderStartPoint = FVector2D(HalfHandleSize.X, (AllottedHeight * 0.5f) - (BarHeight * 0.5));
	SliderEndPoint = FVector2D(AllottedWidth - HalfHandleSize.X, (AllottedHeight * 0.5f) + (BarHeight * 0.5));

	FSlateRect RotatedClippingRect = MyClippingRect;
	FGeometry SliderGeometry = AllottedGeometry;

	// rotate the slider 90deg if it's vertical. The 0 side goes on the bottom, the 1 side on the top.
	if (Orientation == Orient_Vertical)
	{
		// Do this by translating along -X by the width of the geometry, then rotating 90 degreess CCW (left-hand coords)
		FSlateRenderTransform CurrentRenderTransform = TransformCast<FSlateRenderTransform>(Concatenate(Inverse(FVector2D(AllottedWidth, 0)), FQuat2D(FMath::DegreesToRadians(-90.0f))));
		// create a child geometry matching this one, but with the render transform.
		SliderGeometry = AllottedGeometry.MakeChild(
			FVector2D(AllottedWidth, AllottedHeight),
			FSlateLayoutTransform(),
			CurrentRenderTransform, FVector2D::ZeroVector);
		// The clipping rect is already given properly in window space. But we do not support layout rotations, so our local space rendering cannot
		// get the clipping rect into local space properly for the local space clipping we do in the shader.
		// Thus, we transform the clip coords into local space manually, UNDO the render transform so it will clip properly,
		// and then bring the clip coords back into window space where DrawElements expect them.
		RotatedClippingRect = TransformRect(
			Concatenate(
			Inverse(SliderGeometry.GetAccumulatedLayoutTransform()),
			Inverse(CurrentRenderTransform),
			SliderGeometry.GetAccumulatedLayoutTransform()),
			MyClippingRect);
	}

	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	// draw the Background
	float BarWidth = SliderEndPoint.X - SliderStartPoint.X;
	auto BarTopLeft = FVector2D(SliderStartPoint.X, SliderStartPoint.Y);
	auto BarSize = FVector2D(SliderEndPoint.X - SliderStartPoint.X, SliderEndPoint.Y - SliderStartPoint.Y);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		SliderGeometry.ToPaintGeometry(BarTopLeft, BarSize),
		LockedAttribute.Get() ? &Style->DisabledBarImage : &Style->NormalBarImage,
		RotatedClippingRect,
		DrawEffects,
		SliderBarBGColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);

	++LayerId;

	// draw the progress Slider
	BarSize = FVector2D(SliderHandleOffset, SliderEndPoint.Y - SliderStartPoint.Y);
	BarTopLeft = FVector2D(SliderStartPoint.X, SliderStartPoint.Y);
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
	
	for (int i = 0; i < CurrentBookmarks.Num(); i++)
	{
		const float MarkerHandleOffset = CurrentBookmarks[i].Time * SliderLength / TotalValueAttribute.Get();
		FVector2D MarkerSize(6, 12);
		FVector2D MarkerHalfSize = MarkerSize / 2;
		FVector2D MarkerTopLeftPoint = FVector2D(MarkerHandleOffset - MarkerHalfSize.X + 0.5f * Indentation, 0.5f * AllottedHeight - MarkerHalfSize.Y);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			SliderGeometry.ToPaintGeometry(MarkerTopLeftPoint, MarkerSize),
			&Style->NormalThumbImage,
			RotatedClippingRect,
			DrawEffects,
			CurrentBookmarks[i].Color * InWidgetStyle.GetColorAndOpacityTint()
			);
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

	++LayerId;

	if (MarkStart > 0)
	{
		const float MarkerHandleOffset = MarkStart * SliderLength;
		FVector2D MarkerHalfSize(12, 12);
		FVector2D MarkerSize(24, 24);
		FVector2D MarkerTopLeftPoint = FVector2D(MarkerHandleOffset - MarkerHalfSize.X + 0.5f * Indentation, 0.5f * AllottedHeight - MarkerHalfSize.Y);
		// draw the start marker
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			SliderGeometry.ToPaintGeometry(MarkerTopLeftPoint, MarkerSize),
			SUWindowsStyle::Get().GetBrush("UT.Replay.Button.MarkStart"),
			RotatedClippingRect,
			DrawEffects,
			SliderHandleColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
			);
	}

	if (MarkEnd > 0)
	{
		const float MarkerHandleOffset = MarkEnd * SliderLength;
		FVector2D MarkerHalfSize(12, 12);
		FVector2D MarkerSize(24, 24);
		FVector2D MarkerTopLeftPoint = FVector2D(MarkerHandleOffset - MarkerHalfSize.X + 0.5f * Indentation, 0.5f * AllottedHeight - MarkerHalfSize.Y);
		// draw the end marker
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			SliderGeometry.ToPaintGeometry(MarkerTopLeftPoint, MarkerSize),
			SUWindowsStyle::Get().GetBrush("UT.Replay.Button.MarkEnd"),
			RotatedClippingRect,
			DrawEffects,
			SliderHandleColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
			);
	}

	return LayerId;
}

FVector2D SUTProgressSlider::ComputeDesiredSize(float) const
{
	static const FVector2D SUTProgressSliderDesiredSize(16.0f, 16.0f);

	if (Style == nullptr)
	{
		return SUTProgressSliderDesiredSize;
	}

	if (Orientation == Orient_Vertical)
	{
		return FVector2D(Style->NormalThumbImage.ImageSize.Y, SUTProgressSliderDesiredSize.Y);
	}

	return FVector2D(SUTProgressSliderDesiredSize.X, Style->NormalThumbImage.ImageSize.Y);
}

FReply SUTProgressSlider::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && !LockedAttribute.Get())
	{
		OnMouseCaptureBegin.ExecuteIfBound();
		SetCursor(EMouseCursor::GrabHand);

		if (DeferValueAttribute.Get())
		{
			DeferedValue = PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition());
			bSliding = true;
		}
		else
		{
			CommitValue(PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition()));
		}
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SUTProgressSlider::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && HasMouseCapture())
	{
		SetCursor(EMouseCursor::Default);

		if (DeferValueAttribute.Get())
		{
			CommitValue(DeferedValue);
			bSliding = false;
		}
		OnMouseCaptureEnd.ExecuteIfBound();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SUTProgressSlider::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (this->HasMouseCapture() && !LockedAttribute.Get())
	{
		SetCursor(EMouseCursor::GrabHand);

		if (DeferValueAttribute.Get())
		{
			DeferedValue = PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition());
		}
		else
		{
			CommitValue(PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition()));
		}
		return FReply::Handled();
	}
	else
	{
		SetCursor(EMouseCursor::Hand);
	}

	return FReply::Unhandled();
}

void SUTProgressSlider::CommitValue(float NewValue)
{
	if (!ValueAttribute.IsBound())
	{
		ValueAttribute.Set(NewValue);
	}

	OnValueChanged.ExecuteIfBound(NewValue);
}

float SUTProgressSlider::PositionToValue(const FGeometry& MyGeometry, const FVector2D& AbsolutePosition)
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

float SUTProgressSlider::GetValue() const
{
	return ValueAttribute.Get();
}

void SUTProgressSlider::SetValue(const TAttribute<float>& InValueAttribute)
{
	ValueAttribute = InValueAttribute;
}

void SUTProgressSlider::SetIndentHandle(const TAttribute<bool>& InIndentHandle)
{
	IndentHandle = InIndentHandle;
}

void SUTProgressSlider::SetLocked(const TAttribute<bool>& InLocked)
{
	LockedAttribute = InLocked;
}

void SUTProgressSlider::SetOrientation(EOrientation InOrientation)
{
	Orientation = InOrientation;
}

void SUTProgressSlider::SetSliderBarColor(FSlateColor InSliderBarColor)
{
	SliderBarColor = InSliderBarColor;
}

void SUTProgressSlider::SetSliderBarBGColor(FSlateColor InSliderBarColor)
{
	SliderBarBGColor = InSliderBarColor;
}

void SUTProgressSlider::SetSliderHandleColor(FSlateColor InSliderHandleColor)
{
	SliderHandleColor = InSliderHandleColor;
}


#endif