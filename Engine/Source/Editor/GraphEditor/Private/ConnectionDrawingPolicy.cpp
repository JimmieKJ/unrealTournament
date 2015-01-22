// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "ConnectionDrawingPolicy.h"

DEFINE_LOG_CATEGORY(LogConnectionDrawingPolicy);

//////////////////////////////////////////////////////////////////////////
// FGeometryHelper

FVector2D FGeometryHelper::VerticalMiddleLeftOf(const FGeometry& SomeGeometry)
{
	const FVector2D GeometryDrawSize = SomeGeometry.GetDrawSize();
	return FVector2D(
		SomeGeometry.AbsolutePosition.X,
		SomeGeometry.AbsolutePosition.Y + GeometryDrawSize.Y/2 );
}

FVector2D FGeometryHelper::VerticalMiddleRightOf(const FGeometry& SomeGeometry)
{
	const FVector2D GeometryDrawSize = SomeGeometry.GetDrawSize();
	return FVector2D(
		SomeGeometry.AbsolutePosition.X + GeometryDrawSize.X,
		SomeGeometry.AbsolutePosition.Y + GeometryDrawSize.Y/2 );
}

FVector2D FGeometryHelper::CenterOf(const FGeometry& SomeGeometry)
{
	const FVector2D GeometryDrawSize = SomeGeometry.GetDrawSize();
	return SomeGeometry.AbsolutePosition + (GeometryDrawSize * 0.5f);
}

void FGeometryHelper::ConvertToPoints(const FGeometry& Geom, TArray<FVector2D>& Points)
{
	const FVector2D Size = Geom.GetDrawSize();
	const FVector2D Location = Geom.AbsolutePosition;

	int32 Index = Points.AddUninitialized(4);
	Points[Index++] = Location;
	Points[Index++] = Location + FVector2D(0.0f, Size.Y);
	Points[Index++] = Location + FVector2D(Size.X, Size.Y);
	Points[Index++] = Location + FVector2D(Size.X, 0.0f);
}

/** Find the point on line segment from LineStart to LineEnd which is closest to Point */
FVector2D FGeometryHelper::FindClosestPointOnLine(const FVector2D& LineStart, const FVector2D& LineEnd, const FVector2D& TestPoint)
{
	const FVector2D LineVector = LineEnd - LineStart;

	const float A = -FVector2D::DotProduct(LineStart - TestPoint, LineVector);
	const float B = LineVector.SizeSquared();
	const float T = FMath::Clamp<float>(A / B, 0.0f, 1.0f);

	// Generate closest point
	return LineStart + (T * LineVector);
}

FVector2D FGeometryHelper::FindClosestPointOnGeom(const FGeometry& Geom, const FVector2D& TestPoint)
{
	TArray<FVector2D> Points;
	FGeometryHelper::ConvertToPoints(Geom, Points);

	float BestDistanceSquared = MAX_FLT;
	FVector2D BestPoint;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const FVector2D Candidate = FindClosestPointOnLine(Points[i], Points[(i + 1) % Points.Num()], TestPoint);
		const float CandidateDistanceSquared = (Candidate-TestPoint).SizeSquared();
		if (CandidateDistanceSquared < BestDistanceSquared)
		{
			BestPoint = Candidate;
			BestDistanceSquared = CandidateDistanceSquared;
		}
	}

	return BestPoint;
}

/////////////////////////////////////////////////////
// FConnectionDrawingPolicy

FConnectionDrawingPolicy::FConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements)
	: WireLayerID(InBackLayerID)
	, ArrowLayerID(InFrontLayerID)
	, Settings(GetDefault<UGraphEditorSettings>())
	, ZoomFactor(InZoomFactor)
	, ClippingRect(InClippingRect)
	, DrawElementsList(InDrawElements)
{
	ArrowImage = FEditorStyle::GetBrush( TEXT("Graph.Arrow") );
	ArrowRadius = ArrowImage->ImageSize * ZoomFactor * 0.5f;
	MidpointImage = NULL;
	MidpointRadius = FVector2D::ZeroVector;
	HoverDeemphasisDarkFraction = 0.8f;

	BubbleImage = FEditorStyle::GetBrush( TEXT("Graph.ExecutionBubble") );
}

void FConnectionDrawingPolicy::DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FLinearColor& WireColor, float WireThickness, bool bDrawBubbles, bool Bidirectional)
{
	// Draw the spline
	DrawConnection(
		WireLayerID,
		StartPoint,
		EndPoint,
		WireColor,
		WireThickness,
		bDrawBubbles);

	// Draw the arrow
	if (ArrowImage != nullptr)
	{
		FVector2D ArrowPoint = EndPoint - ArrowRadius;

		FSlateDrawElement::MakeBox(
			DrawElementsList,
			ArrowLayerID,
			FPaintGeometry(ArrowPoint, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
			ArrowImage,
			ClippingRect,
			ESlateDrawEffect::None,
			WireColor
			);
	}
}

void FConnectionDrawingPolicy::DrawSplineWithArrow(FGeometry& StartGeom, FGeometry& EndGeom, const FLinearColor& WireColor, float WireThickness, bool bDrawBubbles, bool Bidirectional)
{
	//@TODO: These values should be pushed into the Slate style, they are compensating for a bit of
	// empty space inside of the pin brush images.
	const float StartFudgeX = 4.0f;
	const float EndFudgeX = 4.0f;
	const FVector2D StartPoint = FGeometryHelper::VerticalMiddleRightOf(StartGeom) - FVector2D(StartFudgeX, 0.0f);
	const FVector2D EndPoint = FGeometryHelper::VerticalMiddleLeftOf(EndGeom) - FVector2D(ArrowRadius.X - EndFudgeX, 0);

	DrawSplineWithArrow(StartPoint, EndPoint, WireColor, WireThickness, bDrawBubbles, Bidirectional);
}

// Update the drawing policy with the set of hovered pins (which can be empty)
void FConnectionDrawingPolicy::SetHoveredPins(const TSet< TWeakObjectPtr<UEdGraphPin> >& InHoveredPins, const TArray< TSharedPtr<SGraphPin> >& OverridePins, double HoverTime)
{
	HoveredPins.Empty();

	LastHoverTimeEvent = (OverridePins.Num() > 0) ? 0.0 : HoverTime;

	for (auto PinIt = OverridePins.CreateConstIterator(); PinIt; ++PinIt)
	{
		if (SGraphPin* GraphPin = PinIt->Get())
		{
			HoveredPins.Add(GraphPin->GetPinObj());
		}
	}

	// Convert the widget pointer for hovered pins to be EdGraphPin pointers for their connected nets (both ends of any connection)
	for (auto PinIt = InHoveredPins.CreateConstIterator(); PinIt; ++PinIt)
	{
		if (UEdGraphPin* Pin = PinIt->Get())
		{
			if (Pin->LinkedTo.Num() > 0)
			{
				HoveredPins.Add(Pin);

				for (auto LinkIt = Pin->LinkedTo.CreateConstIterator(); LinkIt; ++LinkIt)
				{
					HoveredPins.Add(*LinkIt);
				}
			}
		}
	}
}

void FConnectionDrawingPolicy::SetMarkedPin(TWeakPtr<SGraphPin> InMarkedPin)
{
	if (InMarkedPin.IsValid())
	{
		LastHoverTimeEvent = 0.0;

		UEdGraphPin* MarkedPin = InMarkedPin.Pin()->GetPinObj();
		HoveredPins.Add(MarkedPin);

		for (auto LinkIt = MarkedPin->LinkedTo.CreateConstIterator(); LinkIt; ++LinkIt)
		{
			HoveredPins.Add(*LinkIt);
		}
	}
}

/** Util to make a 'distance->alpha' table and also return spline length */
float FConnectionDrawingPolicy::MakeSplineReparamTable(const FVector2D& P0, const FVector2D& P0Tangent, const FVector2D& P1, const FVector2D& P1Tangent, FInterpCurve<float>& OutReparamTable)
{
	OutReparamTable.Reset();

	const int32 NumSteps = 10; // TODO: Make this adaptive...

	// Find range of input
	float Param = 0.f;
	const float MaxInput = 1.f;
	const float Interval = (MaxInput - Param)/((float)(NumSteps-1)); 

	// Add first entry, using first point on curve, total distance will be 0
	FVector2D OldSplinePos = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, Param);
	float TotalDist = 0.f;
	OutReparamTable.AddPoint(TotalDist, Param);
	Param += Interval;

	// Then work over rest of points	
	for(int32 i=1; i<NumSteps; i++)
	{
		// Iterate along spline at regular param intervals
		const FVector2D NewSplinePos = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, Param);
		TotalDist += (NewSplinePos - OldSplinePos).Size();
		OldSplinePos = NewSplinePos;

		OutReparamTable.AddPoint(TotalDist, Param);

		Param += Interval;
	}

	return TotalDist;
}

FVector2D FConnectionDrawingPolicy::ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const
{
	return Settings->ComputeSplineTangent(Start, End);
}

void FConnectionDrawingPolicy::DrawConnection( int32 LayerId, const FVector2D& Start, const FVector2D& End, const FLinearColor& InColor, float Thickness, bool bDrawBubbles )
{
	const FVector2D& P0 = Start;
	const FVector2D& P1 = End;

	const FVector2D P0Tangent = ComputeSplineTangent(P0, P1);
	const FVector2D P1Tangent = P0Tangent;

	// Draw the spline itself
	FSlateDrawElement::MakeDrawSpaceSpline(
		DrawElementsList,
		LayerId,
		P0, P0Tangent,
		P1, P1Tangent,
		ClippingRect,
		Thickness,
		ESlateDrawEffect::None,
		InColor
	);

	if (bDrawBubbles || (MidpointImage != NULL))
	{
		// This table maps distance along curve to alpha
		FInterpCurve<float> SplineReparamTable;
		float SplineLength = MakeSplineReparamTable(P0, P0Tangent, P1, P1Tangent, SplineReparamTable);

		// Draw bubbles on the spline
		if (bDrawBubbles)
		{
			const float BubbleSpacing = 64.f * ZoomFactor;
			const float BubbleSpeed = 192.f * ZoomFactor;
			const FVector2D BubbleSize = BubbleImage->ImageSize * ZoomFactor * 0.1f * Thickness;

			float Time = (FPlatformTime::Seconds() - GStartTime);
			const float BubbleOffset = FMath::Fmod(Time * BubbleSpeed, BubbleSpacing);
			const int32 NumBubbles = FMath::CeilToInt(SplineLength/BubbleSpacing);
			for (int32 i = 0; i < NumBubbles; ++i)
			{
				const float Distance = ((float)i * BubbleSpacing) + BubbleOffset;
				if (Distance < SplineLength)
				{
					const float Alpha = SplineReparamTable.Eval(Distance, 0.f);
					FVector2D BubblePos = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, Alpha);
					BubblePos -= (BubbleSize * 0.5f);

					FSlateDrawElement::MakeBox(
						DrawElementsList,
						LayerId,
						FPaintGeometry( BubblePos, BubbleSize, ZoomFactor  ),
						BubbleImage,
						ClippingRect,
						ESlateDrawEffect::None,
						InColor
						);
				}
			}
		}

		// Draw the midpoint image
		if (MidpointImage != NULL)
		{
			// Determine the spline position for the midpoint
			const float MidpointAlpha = SplineReparamTable.Eval(SplineLength * 0.5f, 0.f);
			const FVector2D Midpoint = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha);

			// Approximate the slope at the midpoint (to orient the midpoint image to the spline)
			const FVector2D MidpointPlusE = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha + KINDA_SMALL_NUMBER);
			const FVector2D MidpointMinusE = FMath::CubicInterp(P0, P0Tangent, P1, P1Tangent, MidpointAlpha - KINDA_SMALL_NUMBER);
			const FVector2D SlopeUnnormalized = MidpointPlusE - MidpointMinusE;

			// Draw the arrow
			const FVector2D MidpointDrawPos = Midpoint - MidpointRadius;
			const float AngleInRadians = SlopeUnnormalized.IsNearlyZero() ? 0.0f : FMath::Atan2(SlopeUnnormalized.Y, SlopeUnnormalized.X);

			FSlateDrawElement::MakeRotatedBox(
				DrawElementsList,
				LayerId,
				FPaintGeometry(MidpointDrawPos, MidpointImage->ImageSize * ZoomFactor, ZoomFactor),
				MidpointImage,
				ClippingRect,
				ESlateDrawEffect::None,
				AngleInRadians,
				TOptional<FVector2D>(),
				FSlateDrawElement::RelativeToElement,
				InColor
				);
		}
	}

}

void FConnectionDrawingPolicy::DrawPreviewConnector(const FGeometry& PinGeometry, const FVector2D& StartPoint, const FVector2D& EndPoint, UEdGraphPin* Pin)
{
	float Thickness = 1.0f;
	FLinearColor WireColor = FLinearColor::White;
	bool bDrawBubbles = false;
	bool bBiDirectional = false;
	DetermineWiringStyle(Pin, NULL, /*inout*/ Thickness, /*inout*/ WireColor, /*inout*/ bDrawBubbles, /*inout*/ bBiDirectional);
	DrawSplineWithArrow(StartPoint, EndPoint, WireColor, Thickness, bDrawBubbles, bBiDirectional);
}

void FConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ float& Thickness, /*inout*/ FLinearColor& WireColor, /*inout*/bool& bDrawBubbles, /*inout*/ bool &bBidirectional)
{
}

void FConnectionDrawingPolicy::DetermineLinkGeometry(
	TMap<TSharedRef<SWidget>,
	FArrangedWidget>& PinGeometries,
	FArrangedChildren& ArrangedNodes, 
	TSharedRef<SWidget>& OutputPinWidget,
	UEdGraphPin* OutputPin,
	UEdGraphPin* InputPin,
	/*out*/ FArrangedWidget*& StartWidgetGeometry,
	/*out*/ FArrangedWidget*& EndWidgetGeometry
	)
{
	StartWidgetGeometry = PinGeometries.Find(OutputPinWidget);
	
	if (TSharedRef<SGraphPin>* pTargetWidget = PinToPinWidgetMap.Find(InputPin))
	{
		TSharedRef<SGraphPin> InputWidget = *pTargetWidget;
		EndWidgetGeometry = PinGeometries.Find(InputWidget);
	}
}

void FConnectionDrawingPolicy::Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& PinGeometries, FArrangedChildren& ArrangedNodes)
{
	PinToPinWidgetMap.Empty();
	for (TMap<TSharedRef<SWidget>, FArrangedWidget>::TIterator ConnectorIt(PinGeometries); ConnectorIt; ++ConnectorIt)
	{
		TSharedRef<SWidget> SomePinWidget = ConnectorIt.Key();
		SGraphPin& PinWidget = static_cast<SGraphPin&>(SomePinWidget.Get());

		PinToPinWidgetMap.Add(PinWidget.GetPinObj(), StaticCastSharedRef<SGraphPin>(SomePinWidget));
	}

	for (TMap<TSharedRef<SWidget>, FArrangedWidget>::TIterator ConnectorIt(PinGeometries); ConnectorIt; ++ConnectorIt)
	{
		TSharedRef<SWidget> SomePinWidget = ConnectorIt.Key();
		SGraphPin& PinWidget = static_cast<SGraphPin&>(SomePinWidget.Get());
		UEdGraphPin* ThePin = PinWidget.GetPinObj();

		if (ThePin->Direction == EGPD_Output)
		{
			for (int32 LinkIndex=0; LinkIndex < ThePin->LinkedTo.Num(); ++LinkIndex)
			{
				FArrangedWidget* LinkStartWidgetGeometry = NULL;
				FArrangedWidget* LinkEndWidgetGeometry = NULL;

				UEdGraphPin* TargetPin = ThePin->LinkedTo[LinkIndex];

				DetermineLinkGeometry(PinGeometries, ArrangedNodes, SomePinWidget, ThePin, TargetPin, /*out*/ LinkStartWidgetGeometry, /*out*/ LinkEndWidgetGeometry);

				if(( LinkEndWidgetGeometry && LinkStartWidgetGeometry ) && !IsConnectionCulled( *LinkStartWidgetGeometry, *LinkEndWidgetGeometry ))
				{

					float Thickness = 1.0f;
					FLinearColor WireColor = FLinearColor::White;
					bool bDrawBubbles = false;
					bool bBidirectional = false;

					DetermineWiringStyle(ThePin, TargetPin, /*inout*/ Thickness, /*inout*/ WireColor, /*inout*/ bDrawBubbles, /*inout*/ bBidirectional);
					DrawSplineWithArrow(LinkStartWidgetGeometry->Geometry, LinkEndWidgetGeometry->Geometry, WireColor, Thickness, bDrawBubbles, bBidirectional);
				}
			}
		}
	}
}

bool FConnectionDrawingPolicy::IsConnectionCulled( const FArrangedWidget& StartLink, const FArrangedWidget& EndLink ) const
{
	const float Top		= FMath::Min( StartLink.Geometry.AbsolutePosition.Y, EndLink.Geometry.AbsolutePosition.Y );
	const float Left	= FMath::Min( StartLink.Geometry.AbsolutePosition.X, EndLink.Geometry.AbsolutePosition.X );
	const float Bottom	= FMath::Max( StartLink.Geometry.AbsolutePosition.Y, EndLink.Geometry.AbsolutePosition.Y );
	const float Right	= FMath::Max( StartLink.Geometry.AbsolutePosition.X, EndLink.Geometry.AbsolutePosition.X ); 

	return	Left > ClippingRect.Right || Right < ClippingRect.Left || 
			Bottom < ClippingRect.Top || Top > ClippingRect.Bottom;
}

void FConnectionDrawingPolicy::SetIncompatiblePinDrawState(const TSharedPtr<SGraphPin>& StartPin, const TSet< TSharedRef<SWidget> >& VisiblePins)
{
}

void FConnectionDrawingPolicy::ResetIncompatiblePinDrawState(const TSet< TSharedRef<SWidget> >& VisiblePins)
{
}

void FConnectionDrawingPolicy::ApplyHoverDeemphasis(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ float& Thickness, /*inout*/ FLinearColor& WireColor)
{
	const float FadeInBias = 0.75f; // Time in seconds before the fading starts to occur
	const float FadeInPeriod = 0.6f; // Time in seconds after the bias before the fade is fully complete
	const float TimeFraction = FMath::SmoothStep(0.0f, FadeInPeriod, (float)(FSlateApplication::Get().GetCurrentTime() - LastHoverTimeEvent - FadeInBias));

	const float LightFraction = 0.25f;
	const FLinearColor DarkenedColor(0.0f, 0.0f, 0.0f, 0.5f);
	const FLinearColor LightenedColor(1.0f, 1.0f, 1.0f, 1.0f);

	const bool bContainsBoth = HoveredPins.Contains(InputPin) && HoveredPins.Contains(OutputPin);
	const bool bContainsOutput = HoveredPins.Contains(OutputPin);
	const bool bEmphasize = bContainsBoth || (bContainsOutput && (InputPin == NULL));
	if (bEmphasize)
	{
		Thickness = FMath::Lerp(Thickness, Thickness * ((Thickness < 3.0f) ? 5.0f : 3.0f), TimeFraction);
		WireColor = FMath::Lerp<FLinearColor>(WireColor, LightenedColor, LightFraction * TimeFraction);
	}
	else
	{
		WireColor = FMath::Lerp<FLinearColor>(WireColor, DarkenedColor, HoverDeemphasisDarkFraction * TimeFraction);
	}
}
