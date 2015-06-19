// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
 
SBorder::SBorder()
	: BorderImage( FCoreStyle::Get().GetBrush( "Border" ) )
	, BorderBackgroundColor( FLinearColor::White )
	, DesiredSizeScale(FVector2D(1,1))
{ }


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SBorder::Construct( const SBorder::FArguments& InArgs )
{
	ContentScale = InArgs._ContentScale;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	DesiredSizeScale = InArgs._DesiredSizeScale;

	ShowDisabledEffect = InArgs._ShowEffectWhenDisabled;

	ChildSlot
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(InArgs._Padding)
	[
		InArgs._Content.Widget
	];

	BorderImage = InArgs._BorderImage;
	BorderBackgroundColor = InArgs._BorderBackgroundColor;
	ForegroundColor = InArgs._ForegroundColor;
	MouseButtonDownHandler = InArgs._OnMouseButtonDown;
	MouseButtonUpHandler = InArgs._OnMouseButtonUp;
	MouseMoveHandler = InArgs._OnMouseMove;
	MouseDoubleClickHandler = InArgs._OnMouseDoubleClick;
}


/**
 * Sets the content for this border
 *
 * @param	InContent	The widget to use as content for the border
 */
void SBorder::SetContent( TSharedRef< SWidget > InContent )
{
	ChildSlot
	[
		InContent
	];
}

const TSharedRef< SWidget >& SBorder::GetContent() const
{
	return ChildSlot.GetWidget();
}

/** Clears out the content for the border */
void SBorder::ClearContent()
{
	ChildSlot.DetachWidget();
}

int32 SBorder::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	bool bEnabled = false;
	const FSlateBrush* BrushResource = BorderImage.Get();
		
	bEnabled = ShouldBeEnabled( bParentEnabled );
	bool bShowDisabledEffect = ShowDisabledEffect.Get();
	ESlateDrawEffect::Type DrawEffects = bShowDisabledEffect && !bEnabled ? ESlateDrawEffect::DisabledEffect : ESlateDrawEffect::None;

	if ( BrushResource && BrushResource->DrawAs != ESlateBrushDrawType::NoDrawType )
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			BrushResource,
			MyClippingRect,
			DrawEffects,
			BrushResource->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint() * BorderBackgroundColor.Get().GetColor( InWidgetStyle )
		);
	}

	FWidgetStyle CompoundedWidgetStyle = FWidgetStyle(InWidgetStyle)
		.BlendColorAndOpacityTint(ColorAndOpacity.Get())
		.SetForegroundColor( ForegroundColor.Get() );

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect.IntersectionWith( AllottedGeometry.GetClippingRect() ), OutDrawElements, LayerId, CompoundedWidgetStyle, bEnabled );
}


/**
 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SBorder::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseButtonDownHandler.IsBound() )
	{
		// If a handler is assigned, call it.
		return MouseButtonDownHandler.Execute(MyGeometry, MouseEvent);
	}
	else
	{
		// otherwise the event is unhandled.
		return FReply::Unhandled();
	}
}

/**
 * The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SBorder::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseButtonUpHandler.IsBound() )
	{
		// If a handler is assigned, call it.
		return MouseButtonUpHandler.Execute(MyGeometry, MouseEvent);
	}
	else
	{
		// otherwise the event is unhandled.
		return FReply::Unhandled();
	}
}

/**
 * The system calls this method to notify the widget that a mouse moved within it. This event is bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SBorder::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseMoveHandler.IsBound() )
	{
		// A valid handler is assigned for mouse move; let it handle the event.
		return MouseMoveHandler.Execute( MyGeometry, MouseEvent );
	}
	else
	{
		// otherwise the event is unhandled
		return FReply::Unhandled();
	}
}

/**
 * Called when a mouse button is double clicked
 *
 * @param  InMyGeometry  Widget geometry
 * @param  InMouseEvent  Mouse button event
 *
 * @return  Returns whether the event was handled, along with other possible actions
 */
FReply SBorder::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseDoubleClickHandler.IsBound() )
	{
		// A valid handler is assigned; let it handle the event.
		return MouseDoubleClickHandler.Execute( MyGeometry, MouseEvent );
	}
	else
	{
		// otherwise the event is unhandled
		return FReply::Unhandled();
	}
	return FReply::Unhandled();
}

FVector2D SBorder::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return DesiredSizeScale.Get() * SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
}

void SBorder::SetBorderBackgroundColor(const TAttribute<FSlateColor>& InColorAndOpacity)
{
	BorderBackgroundColor = InColorAndOpacity;
}

void SBorder::SetDesiredSizeScale(const TAttribute<FVector2D>& InDesiredSizeScale)
{
	DesiredSizeScale = InDesiredSizeScale;
}

void SBorder::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.HAlignment = HAlign;
}

void SBorder::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.VAlignment = VAlign;
}

void SBorder::SetPadding(const TAttribute<FMargin>& InPadding)
{
	ChildSlot.SlotPadding = InPadding;
}

void SBorder::SetShowEffectWhenDisabled(const TAttribute<bool>& InShowEffectWhenDisabled)
{
	ShowDisabledEffect = InShowEffectWhenDisabled;
}

void SBorder::SetBorderImage(const TAttribute<const FSlateBrush*>& InBorderImage)
{
	BorderImage = InBorderImage;
}

void SBorder::SetOnMouseButtonDown(FPointerEventHandler EventHandler)
{
	MouseButtonDownHandler = EventHandler;
}

void SBorder::SetOnMouseButtonUp(FPointerEventHandler EventHandler)
{
	MouseButtonUpHandler = EventHandler;
}

void SBorder::SetOnMouseMove(FPointerEventHandler EventHandler)
{
	MouseMoveHandler = EventHandler;
}

void SBorder::SetOnMouseDoubleClick(FPointerEventHandler EventHandler)
{
	MouseDoubleClickHandler = EventHandler;
}
