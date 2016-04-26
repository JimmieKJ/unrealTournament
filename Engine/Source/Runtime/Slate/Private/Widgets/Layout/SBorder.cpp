// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

static FName SBorderTypeName("SBorder");

SBorder::SBorder()
	: BorderImage( FCoreStyle::Get().GetBrush( "Border" ) )
	, BorderBackgroundColor( FLinearColor::White )
	, DesiredSizeScale(FVector2D(1,1))
{
}

void SBorder::Construct( const SBorder::FArguments& InArgs )
{
	// Only do this if we're exactly an SBorder
	if ( GetType() == SBorderTypeName )
	{
		bCanTick = false;
		bCanSupportFocus = false;
	}

	ContentScale = InArgs._ContentScale;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	DesiredSizeScale = InArgs._DesiredSizeScale;

	ShowDisabledEffect = InArgs._ShowEffectWhenDisabled;

	BorderImage = InArgs._BorderImage;
	BorderBackgroundColor = InArgs._BorderBackgroundColor;
	ForegroundColor = InArgs._ForegroundColor;
	MouseButtonDownHandler = InArgs._OnMouseButtonDown;
	MouseButtonUpHandler = InArgs._OnMouseButtonUp;
	MouseMoveHandler = InArgs._OnMouseMove;
	MouseDoubleClickHandler = InArgs._OnMouseDoubleClick;

	ChildSlot
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(InArgs._Padding)
	[
		InArgs._Content.Widget
	];
}

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

void SBorder::ClearContent()
{
	ChildSlot.DetachWidget();
}

int32 SBorder::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FSlateBrush* BrushResource = BorderImage.Get();
		
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const bool bShowDisabledEffect = ShowDisabledEffect.Get();
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
