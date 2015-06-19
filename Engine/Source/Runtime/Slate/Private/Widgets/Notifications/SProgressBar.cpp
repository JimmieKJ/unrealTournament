// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


void SProgressBar::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	MarqueeOffset = 0.0f;

	Style = InArgs._Style;

	SetPercent(InArgs._Percent);
	BarFillType = InArgs._BarFillType;
	
	BackgroundImage = InArgs._BackgroundImage;
	FillImage = InArgs._FillImage;
	MarqueeImage = InArgs._MarqueeImage;
	
	FillColorAndOpacity = InArgs._FillColorAndOpacity;
	BorderPadding = InArgs._BorderPadding;

	CurrentTickRate = 0.0f;
	MinimumTickRate = InArgs._RefreshRate;

	ActiveTimerHandle = RegisterActiveTimer(CurrentTickRate, FWidgetActiveTimerDelegate::CreateSP(this, &SProgressBar::ActiveTick));
}

void SProgressBar::SetPercent(TAttribute< TOptional<float> > InPercent)
{
	Percent = InPercent;
}

void SProgressBar::SetStyle(const FProgressBarStyle* InStyle)
{
	Style = InStyle;
}

void SProgressBar::SetBarFillType(EProgressBarFillType::Type InBarFillType)
{
	BarFillType = InBarFillType;
}

void SProgressBar::SetFillColorAndOpacity(TAttribute< FSlateColor > InFillColorAndOpacity)
{
	FillColorAndOpacity = InFillColorAndOpacity;
}

void SProgressBar::SetBorderPadding(TAttribute< FVector2D > InBorderPadding)
{
	BorderPadding = InBorderPadding;
}

void SProgressBar::SetBackgroundImage(const FSlateBrush* InBackgroundImage)
{
	BackgroundImage = InBackgroundImage;
}

void SProgressBar::SetFillImage(const FSlateBrush* InFillImage)
{
	FillImage = InFillImage;
}

void SProgressBar::SetMarqueeImage(const FSlateBrush* InMarqueeImage)
{
	MarqueeImage = InMarqueeImage;
}

const FSlateBrush* SProgressBar::GetBackgroundImage() const
{
	return BackgroundImage ? BackgroundImage : &Style->BackgroundImage;
}

const FSlateBrush* SProgressBar::GetFillImage() const
{
	return FillImage ? FillImage : &Style->FillImage;
}

const FSlateBrush* SProgressBar::GetMarqueeImage() const
{
	return MarqueeImage ? MarqueeImage : &Style->MarqueeImage;
}

int32 SProgressBar::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// Used to track the layer ID we will return.
	int32 RetLayerId = LayerId;

	bool bEnabled = ShouldBeEnabled( bParentEnabled );
	const ESlateDrawEffect::Type DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	const FSlateBrush* CurrentFillImage = GetFillImage();
	
	const FColor FillColorAndOpacitySRGB( InWidgetStyle.GetColorAndOpacityTint() * FillColorAndOpacity.Get().GetColor(InWidgetStyle) * CurrentFillImage->GetTint( InWidgetStyle ) );
	const FColor ColorAndOpacitySRGB = InWidgetStyle.GetColorAndOpacityTint();

	TOptional<float> ProgressFraction = Percent.Get();	

	// Paint inside the border only. 
	// Pre-snap the clipping rect to try and reduce common jitter, since the padding is typically only a single pixel.
	FSlateRect SnappedClippingRect = FSlateRect(FMath::RoundToInt(MyClippingRect.Left), FMath::RoundToInt(MyClippingRect.Top), FMath::RoundToInt(MyClippingRect.Right), FMath::RoundToInt(MyClippingRect.Bottom));
	const FSlateRect ForegroundClippingRect = SnappedClippingRect.InsetBy(FMargin(BorderPadding.Get().X, BorderPadding.Get().Y));
	
	const FSlateBrush* CurrentBackgroundImage = GetBackgroundImage();

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		RetLayerId++,
		AllottedGeometry.ToPaintGeometry(),
		CurrentBackgroundImage,
		SnappedClippingRect,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * CurrentBackgroundImage->GetTint( InWidgetStyle )
	);	
	
	if( ProgressFraction.IsSet() )
	{
		const float ClampedFraction = FMath::Clamp(ProgressFraction.GetValue(), 0.0f, 1.0f);

		switch (BarFillType)
		{
			case EProgressBarFillType::RightToLeft:
			{
				FSlateRect ClippedAllotedGeometry = FSlateRect(AllottedGeometry.AbsolutePosition, AllottedGeometry.AbsolutePosition + AllottedGeometry.Size * AllottedGeometry.Scale);
				ClippedAllotedGeometry.Left = ClippedAllotedGeometry.Right - ClippedAllotedGeometry.GetSize().X * ClampedFraction;

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D::ZeroVector,
						FVector2D( AllottedGeometry.Size.X, AllottedGeometry.Size.Y )),
					CurrentFillImage,
					ForegroundClippingRect.IntersectionWith(ClippedAllotedGeometry),
					DrawEffects,
					FillColorAndOpacitySRGB
					);
				break;
			}
			case EProgressBarFillType::FillFromCenter:
			{
				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D( (AllottedGeometry.Size.X * 0.5f) - ((AllottedGeometry.Size.X * ( ClampedFraction ))*0.5), 0.0f),
						FVector2D( AllottedGeometry.Size.X * ( ClampedFraction ) , AllottedGeometry.Size.Y )),
					CurrentFillImage,
					ForegroundClippingRect,
					DrawEffects,
					FillColorAndOpacitySRGB
					);
				break;
			}
			case EProgressBarFillType::TopToBottom:
			{
				FSlateRect ClippedAllotedGeometry = FSlateRect(AllottedGeometry.AbsolutePosition, AllottedGeometry.AbsolutePosition + AllottedGeometry.Size * AllottedGeometry.Scale);
				ClippedAllotedGeometry.Bottom = ClippedAllotedGeometry.Top + ClippedAllotedGeometry.GetSize().Y * ClampedFraction;

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D::ZeroVector,
						FVector2D( AllottedGeometry.Size.X, AllottedGeometry.Size.Y )),
					CurrentFillImage,
					ForegroundClippingRect.IntersectionWith(ClippedAllotedGeometry),
					DrawEffects,
					FillColorAndOpacitySRGB
					);
				break;
			}
			case EProgressBarFillType::BottomToTop:
			{
				FSlateRect ClippedAllotedGeometry = FSlateRect(AllottedGeometry.AbsolutePosition, AllottedGeometry.AbsolutePosition + AllottedGeometry.Size * AllottedGeometry.Scale);
				ClippedAllotedGeometry.Top = ClippedAllotedGeometry.Bottom - ClippedAllotedGeometry.GetSize().Y * ClampedFraction;

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D::ZeroVector,
						FVector2D( AllottedGeometry.Size.X, AllottedGeometry.Size.Y )),
					CurrentFillImage,
					ForegroundClippingRect.IntersectionWith(ClippedAllotedGeometry),
					DrawEffects,
					FillColorAndOpacitySRGB
					);
				break;
			}
			case EProgressBarFillType::LeftToRight:
			default:
			{
				FSlateRect ClippedAllotedGeometry = FSlateRect(AllottedGeometry.AbsolutePosition, AllottedGeometry.AbsolutePosition + AllottedGeometry.Size * AllottedGeometry.Scale);
				ClippedAllotedGeometry.Right = ClippedAllotedGeometry.Left + ClippedAllotedGeometry.GetSize().X * ClampedFraction;

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D::ZeroVector,
						FVector2D( AllottedGeometry.Size.X, AllottedGeometry.Size.Y )),
					CurrentFillImage,
					ForegroundClippingRect.IntersectionWith(ClippedAllotedGeometry),
					DrawEffects,
					FillColorAndOpacitySRGB
					);
				break;
			}
		}
	}
	else
	{
		const FSlateBrush* CurrentMarqueeImage = GetMarqueeImage();
		
		// Draw Marquee
		const float MarqueeAnimOffset = CurrentMarqueeImage->ImageSize.X * MarqueeOffset;
		const float MarqueeImageSize = CurrentMarqueeImage->ImageSize.X;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			RetLayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D( MarqueeAnimOffset - MarqueeImageSize, 0.0f ),
				FVector2D( AllottedGeometry.Size.X + MarqueeImageSize, AllottedGeometry.Size.Y )),
			CurrentMarqueeImage,
			ForegroundClippingRect,
			DrawEffects,
			ColorAndOpacitySRGB
			);

	}

	return RetLayerId - 1;
}

/**
 * Computes the desired size of this widget (SWidget)
 *
 * @return  The widget's desired size
 */
FVector2D SProgressBar::ComputeDesiredSize( float ) const
{
	return GetMarqueeImage()->ImageSize;
}

void SProgressBar::SetActiveTimerTickRate(float TickRate)
{
	if (CurrentTickRate != TickRate || !ActiveTimerHandle.IsValid())
	{
		CurrentTickRate = TickRate;

		TSharedPtr<FActiveTimerHandle> SharedActiveTimerHandle = ActiveTimerHandle.Pin();
		if (SharedActiveTimerHandle.IsValid())
		{
			UnRegisterActiveTimer(SharedActiveTimerHandle.ToSharedRef());
		}

		ActiveTimerHandle = RegisterActiveTimer(TickRate, FWidgetActiveTimerDelegate::CreateSP(this, &SProgressBar::ActiveTick));
	}
}

EActiveTimerReturnType SProgressBar::ActiveTick(double InCurrentTime, float InDeltaTime)
{
	MarqueeOffset = InCurrentTime - FMath::FloorToDouble(InCurrentTime);
	
	TOptional<float> PrecentFracton = Percent.Get();
	if (PrecentFracton.IsSet())
	{
		SetActiveTimerTickRate(MinimumTickRate);
	}
	else
	{
		SetActiveTimerTickRate(0.0f);
	}

	return EActiveTimerReturnType::Continue;
}


