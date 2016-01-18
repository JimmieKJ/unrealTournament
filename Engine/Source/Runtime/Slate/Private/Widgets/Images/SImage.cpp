// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SImage::Construct( const FArguments& InArgs )
{
	Image = InArgs._Image;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	OnMouseButtonDownHandler = InArgs._OnMouseButtonDown;
}


int32 SImage::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FSlateBrush* ImageBrush = Image.Get();

	if ((ImageBrush != nullptr) && (ImageBrush->DrawAs != ESlateBrushDrawType::NoDrawType))
	{
		const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
		const uint32 DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const FLinearColor FinalColorAndOpacity( InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get().GetColor(InWidgetStyle) * ImageBrush->GetTint( InWidgetStyle ) );

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ImageBrush, MyClippingRect, DrawEffects, FinalColorAndOpacity );
	}
	return LayerId;
}
	
/**
 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SImage::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( OnMouseButtonDownHandler.IsBound() )
	{
		// If a handler is assigned, call it.
		return OnMouseButtonDownHandler.Execute(MyGeometry, MouseEvent);
	}
	else
	{
		// otherwise the event is unhandled.
		return FReply::Unhandled();
	}	
}

/**
 * An Image's desired size is whatever the image looks best as. This is decided on a case by case basis via the Width and Height properties.
 */
FVector2D SImage::ComputeDesiredSize( float ) const
{
	const FSlateBrush* ImageBrush = Image.Get();
	if (ImageBrush != nullptr)
	{
		return ImageBrush->ImageSize;
	}
	return FVector2D::ZeroVector;
}

void SImage::SetColorAndOpacity( const TAttribute<FSlateColor>& InColorAndOpacity )
{
	if ( !ColorAndOpacity.IdenticalTo(InColorAndOpacity) )
	{
		ColorAndOpacity = InColorAndOpacity;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SImage::SetColorAndOpacity( FLinearColor InColorAndOpacity )
{
	if ( ColorAndOpacity.IsBound() || ColorAndOpacity.Get() != InColorAndOpacity )
	{
		ColorAndOpacity = InColorAndOpacity;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SImage::SetImage(TAttribute<const FSlateBrush*> InImage)
{
	Image = InImage;
	Invalidate(EInvalidateWidget::Layout);
}

void SImage::SetOnMouseButtonDown(FPointerEventHandler EventHandler)
{
	OnMouseButtonDownHandler = EventHandler;
}
