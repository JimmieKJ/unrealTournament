// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"

void FSlateDrawElement::Init(uint32 InLayer, const FPaintGeometry& PaintGeometry, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects)
{
	RenderTransform = PaintGeometry.GetAccumulatedRenderTransform();
	Position = PaintGeometry.DrawPosition;
	LocalSize = PaintGeometry.GetLocalSize();
	ClippingRect = InClippingRect;
	Layer = InLayer;
	Scale = PaintGeometry.DrawScale;
	DrawEffects = InDrawEffects;
	extern SLATECORE_API TOptional<FShortRect> GSlateScissorRect;
	ScissorRect = GSlateScissorRect;
}

void FSlateDrawElement::MakeDebugQuad( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FSlateRect& InClippingRect)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, ESlateDrawEffect::None);
	DrawElt.ElementType = ET_DebugQuad;
}


void FSlateDrawElement::MakeBox( 
	FSlateWindowElementList& ElementList,
	uint32 InLayer, 
	const FPaintGeometry& PaintGeometry, 
	const FSlateBrush* InBrush, 
	const FSlateRect& InClippingRect, 
	ESlateDrawEffect::Type InDrawEffects, 
	const FLinearColor& InTint )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = (InBrush->DrawAs == ESlateBrushDrawType::Border) ? ET_Border : ET_Box;
	DrawElt.DataPayload.SetBoxPayloadProperties( InBrush, InTint );
}

void FSlateDrawElement::MakeRotatedBox( 
	FSlateWindowElementList& ElementList,
	uint32 InLayer, 
	const FPaintGeometry& PaintGeometry, 
	const FSlateBrush* InBrush, 
	const FSlateRect& InClippingRect, 
	ESlateDrawEffect::Type InDrawEffects, 
	float Angle,
	TOptional<FVector2D> InRotationPoint,
	ERotationSpace RotationSpace,
	const FLinearColor& InTint )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = (InBrush->DrawAs == ESlateBrushDrawType::Border) ? ET_Border : ET_Box;

	FVector2D RotationPoint = GetRotationPoint( PaintGeometry, InRotationPoint, RotationSpace );
	DrawElt.DataPayload.SetRotatedBoxPayloadProperties( InBrush, Angle, RotationPoint, InTint );
}


void FSlateDrawElement::MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FString& InText, const int32 StartIndex, const int32 EndIndex, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Text;
	DrawElt.DataPayload.SetTextPayloadProperties( FString( EndIndex - StartIndex, *InText + StartIndex ), InFontInfo, InTint );
}


void FSlateDrawElement::MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FString& InText, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Text;
	DrawElt.DataPayload.SetTextPayloadProperties( InText, InFontInfo, InTint );
}


void FSlateDrawElement::MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FText& InText, const FSlateFontInfo& InFontInfo, const FSlateRect& InClippingRect,ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Text;
	DrawElt.DataPayload.SetTextPayloadProperties( InText.ToString(), InFontInfo, InTint );
}

void FSlateDrawElement::MakeGradient( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TArray<FSlateGradientStop> InGradientStops, EOrientation InGradientType, const FSlateRect& InClippingRect, ESlateDrawEffect::Type InDrawEffects, bool bGammaCorrect )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Gradient;
	DrawElt.DataPayload.SetGradientPayloadProperties( InGradientStops, InGradientType, bGammaCorrect );
}


void FSlateDrawElement::MakeSpline( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, float InThickness, ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Spline;
	DrawElt.DataPayload.SetSplinePayloadProperties( InStart, InStartDir, InEnd, InEndDir, InThickness, InTint );
}


void FSlateDrawElement::MakeDrawSpaceSpline( FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, float InThickness, ESlateDrawEffect::Type InDrawEffects, const FColor& InTint )
{
	MakeSpline( ElementList, InLayer, FPaintGeometry(), InStart, InStartDir, InEnd, InEndDir, InClippingRect, InThickness, InDrawEffects, InTint );
}


void FSlateDrawElement::MakeLines( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const TArray<FVector2D>& Points, const FSlateRect InClippingRect, ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint, bool bAntialias )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Line;
	DrawElt.DataPayload.SetLinesPayloadProperties( Points, InTint, bAntialias, ESlateLineJoinType::Sharp );
}


void FSlateDrawElement::MakeViewport( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TSharedPtr<const ISlateViewport> Viewport, const FSlateRect& InClippingRect, bool bGammaCorrect, bool bAllowBlending, ESlateDrawEffect::Type InDrawEffects, const FLinearColor& InTint )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, PaintGeometry, InClippingRect, InDrawEffects);
	DrawElt.ElementType = ET_Viewport;
	DrawElt.DataPayload.SetViewportPayloadProperties( Viewport, InTint, bGammaCorrect, bAllowBlending );
}


void FSlateDrawElement::MakeCustom( FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer )
{
	FSlateDrawElement& DrawElt = ElementList.AddUninitialized();
	DrawElt.Init(InLayer, FPaintGeometry(), FSlateRect(1,1,1,1), ESlateDrawEffect::None);
	DrawElt.RenderTransform = FSlateRenderTransform();
	DrawElt.ElementType = ET_Custom;
	DrawElt.DataPayload.SetCustomDrawerPayloadProperties( CustomDrawer );
}

FVector2D FSlateDrawElement::GetRotationPoint(const FPaintGeometry& PaintGeometry, const TOptional<FVector2D>& UserRotationPoint, ERotationSpace RotationSpace)
{
	FVector2D RotationPoint(0, 0);

	const FVector2D& LocalSize = PaintGeometry.GetLocalSize();

	switch (RotationSpace)
	{
	case RelativeToElement:
	{
		// If the user did not specify a rotation point, we rotate about the center of the element
		RotationPoint = UserRotationPoint.Get(LocalSize * 0.5f);
	}
		break;
	case RelativeToWorld:
	{
		// its in world space, must convert the point to local space.
		RotationPoint = TransformPoint(Inverse(PaintGeometry.GetAccumulatedRenderTransform()), UserRotationPoint.Get(FVector2D::ZeroVector));
	}
		break;
	default:
		check(0);
		break;
	}

	return RotationPoint;
}

FSlateWindowElementList::FDeferredPaint::FDeferredPaint( const TSharedRef<const SWidget>& InWidgetToPaint, const FPaintArgs& InArgs, const FGeometry InAllottedGeometry, const FSlateRect InMyClippingRect, const FWidgetStyle& InWidgetStyle, bool InParentEnabled )
: WidgetToPaintPtr( InWidgetToPaint )
, Args( InArgs )
, AllottedGeometry( InAllottedGeometry )
, MyClippingRect( InMyClippingRect )
, WidgetStyle( InWidgetStyle )
, bParentEnabled( InParentEnabled )
{
}


int32 FSlateWindowElementList::FDeferredPaint::ExecutePaint( int32 LayerId, FSlateWindowElementList& OutDrawElements ) const
{
	TSharedPtr<const SWidget> WidgetToPaint = WidgetToPaintPtr.Pin();
	if ( WidgetToPaint.IsValid() )
	{
		return WidgetToPaint->Paint( Args, AllottedGeometry, OutDrawElements.GetWindow()->GetClippingRectangleInWindow(), OutDrawElements, LayerId, WidgetStyle, bParentEnabled );
	}

	return LayerId;
}


void FSlateWindowElementList::QueueDeferredPainting( const FDeferredPaint& InDeferredPaint )
{
	DeferredPaintList.Add( MakeShareable( new FDeferredPaint( InDeferredPaint ) ) );
}


int32 FSlateWindowElementList::PaintDeferred( int32 LayerId )
{
	for ( int32 i = 0; i < DeferredPaintList.Num(); ++i )
	{
		const TSharedRef< FDeferredPaint >& Args = DeferredPaintList[ i ];

		LayerId = Args->ExecutePaint( LayerId, *this );
	}

	return LayerId;
}
