// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "SDesignSurface.h"

#define LOCTEXT_NAMESPACE "UMG"

struct FZoomLevelEntry
{
public:
	FZoomLevelEntry(float InZoomAmount, const FText& InDisplayText, EGraphRenderingLOD::Type InLOD)
		: DisplayText(FText::Format(LOCTEXT("Zoom", "Zoom {0}"), InDisplayText))
		, ZoomAmount(InZoomAmount)
		, LOD(InLOD)
	{
	}

public:
	FText DisplayText;
	float ZoomAmount;
	EGraphRenderingLOD::Type LOD;
};

struct FFixedZoomLevelsContainer : public FZoomLevelsContainer
{
	FFixedZoomLevelsContainer()
	{
		// Initialize zoom levels if not done already
		if ( ZoomLevels.Num() == 0 )
		{
			ZoomLevels.Reserve(26);
			ZoomLevels.Add(FZoomLevelEntry(0.150f, FText::FromString("-10"), EGraphRenderingLOD::LowestDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.175f, FText::FromString("-9"), EGraphRenderingLOD::LowestDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.200f, FText::FromString("-8"), EGraphRenderingLOD::LowestDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.225f, FText::FromString("-7"), EGraphRenderingLOD::LowDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.250f, FText::FromString("-6"), EGraphRenderingLOD::LowDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.375f, FText::FromString("-5"), EGraphRenderingLOD::MediumDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.500f, FText::FromString("-4"), EGraphRenderingLOD::MediumDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.675f, FText::FromString("-3"), EGraphRenderingLOD::MediumDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.750f, FText::FromString("-2"), EGraphRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FZoomLevelEntry(0.875f, FText::FromString("-1"), EGraphRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FZoomLevelEntry(1.000f, FText::FromString("1:1"), EGraphRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FZoomLevelEntry(1.250f, FText::FromString("+1"), EGraphRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FZoomLevelEntry(1.500f, FText::FromString("+2"), EGraphRenderingLOD::DefaultDetail));
			ZoomLevels.Add(FZoomLevelEntry(1.750f, FText::FromString("+3"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(2.000f, FText::FromString("+4"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(2.250f, FText::FromString("+5"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(2.500f, FText::FromString("+6"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(2.750f, FText::FromString("+7"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(3.000f, FText::FromString("+8"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(3.250f, FText::FromString("+9"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(3.500f, FText::FromString("+10"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(4.000f, FText::FromString("+11"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(5.000f, FText::FromString("+12"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(6.000f, FText::FromString("+13"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(7.000f, FText::FromString("+14"), EGraphRenderingLOD::FullyZoomedIn));
			ZoomLevels.Add(FZoomLevelEntry(8.000f, FText::FromString("+15"), EGraphRenderingLOD::FullyZoomedIn));
		}
	}

	float GetZoomAmount(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].ZoomAmount;
	}

	int32 GetNearestZoomLevel(float InZoomAmount) const override
	{
		for ( int32 ZoomLevelIndex=0; ZoomLevelIndex < GetNumZoomLevels(); ++ZoomLevelIndex )
		{
			if ( InZoomAmount <= GetZoomAmount(ZoomLevelIndex) )
			{
				return ZoomLevelIndex;
			}
		}

		return GetDefaultZoomLevel();
	}

	FText GetZoomText(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].DisplayText;
	}

	int32 GetNumZoomLevels() const override
	{
		return ZoomLevels.Num();
	}

	int32 GetDefaultZoomLevel() const override
	{
		return 10;
	}

	EGraphRenderingLOD::Type GetLOD(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].LOD;
	}

	static TArray<FZoomLevelEntry> ZoomLevels;
};

TArray<FZoomLevelEntry> FFixedZoomLevelsContainer::ZoomLevels;

/////////////////////////////////////////////////////
// SDesignSurface

void SDesignSurface::Construct(const FArguments& InArgs)
{
	if ( !ZoomLevels.IsValid() )
	{
		ZoomLevels = new FFixedZoomLevelsContainer();
	}
	ZoomLevel = ZoomLevels->GetDefaultZoomLevel();
	PreviousZoomLevel = ZoomLevels->GetDefaultZoomLevel();
	PostChangedZoom();
	SnapGridSize = 16.0f;
	AllowContinousZoomInterpolation = InArgs._AllowContinousZoomInterpolation;
	bIsPanning = false;

	ViewOffset = FVector2D::ZeroVector;

	ZoomLevelFade = FCurveSequence(0.0f, 1.0f);
	ZoomLevelFade.Play( this->AsShared() );

	ZoomLevelGraphFade = FCurveSequence(0.0f, 0.5f);
	ZoomLevelGraphFade.Play( this->AsShared() );

	bDeferredZoomToExtents = false;

	bAllowContinousZoomInterpolation = false;
	bTeleportInsteadOfScrollingWhenZoomingToFit = false;

	bRequireControlToOverZoom = false;

	ZoomTargetTopLeft = FVector2D::ZeroVector;
	ZoomTargetBottomRight = FVector2D::ZeroVector;

	ZoomToFitPadding = FVector2D(100, 100);
	TotalGestureMagnify = 0.0f;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

EActiveTimerReturnType SDesignSurface::HandleZoomToFit( double InCurrentTime, float InDeltaTime )
{
	const FVector2D DesiredViewCenter = ( ZoomTargetTopLeft + ZoomTargetBottomRight ) * 0.5f;
	const bool bDoneScrolling = ScrollToLocation(CachedGeometry, DesiredViewCenter, bTeleportInsteadOfScrollingWhenZoomingToFit ? 1000.0f : InDeltaTime);
	const bool bDoneZooming = ZoomToLocation(CachedGeometry.Size, ZoomTargetBottomRight - ZoomTargetTopLeft, bDoneScrolling);

	if (bDoneZooming && bDoneScrolling)
	{
		// One final push to make sure we're centered in the end
		ViewOffset = DesiredViewCenter - ( 0.5f * CachedGeometry.Scale * CachedGeometry.Size / GetZoomAmount() );

		ZoomTargetTopLeft = FVector2D::ZeroVector;
		ZoomTargetBottomRight = FVector2D::ZeroVector;

		return EActiveTimerReturnType::Stop;
	}
	
	return EActiveTimerReturnType::Continue;
}

void SDesignSurface::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	CachedGeometry = AllottedGeometry;
	
	// Zoom to extents
	FSlateRect Bounds = ComputeAreaBounds();
	if ( bDeferredZoomToExtents )
	{
		bDeferredZoomToExtents = false;
		ZoomTargetTopLeft = FVector2D(Bounds.Left, Bounds.Top);
		ZoomTargetBottomRight = FVector2D(Bounds.Right, Bounds.Bottom);

		if (!ActiveTimerHandle.IsValid())
		{
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SDesignSurface::HandleZoomToFit));
		}
	}
}

int32 SDesignSurface::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* BackgroundImage = FEditorStyle::GetBrush(TEXT("Graph.Panel.SolidBackground"));
	PaintBackgroundAsLines(BackgroundImage, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId);

	SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	return LayerId;
}

FReply SDesignSurface::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);

	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		bIsPanning = false;
	}

	return FReply::Unhandled();
}

FReply SDesignSurface::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);

	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		bIsPanning = false;
	}

	return FReply::Unhandled();
}

FReply SDesignSurface::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const bool bIsRightMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton);
	const bool bIsLeftMouseButtonDown = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
	const FModifierKeysState ModifierKeysState = FSlateApplication::Get().GetModifierKeys();

	if ( HasMouseCapture() )
	{
		const FVector2D CursorDelta = MouseEvent.GetCursorDelta();

		//const bool bShouldZoom = bIsRightMouseButtonDown && ( bIsLeftMouseButtonDown || ModifierKeysState.IsAltDown() || FSlateApplication::Get().IsUsingTrackpad() );
		if ( bIsRightMouseButtonDown )
		{
			FReply ReplyState = FReply::Handled();

			bIsPanning = true;
			ViewOffset -= CursorDelta / GetZoomAmount();

			return ReplyState;
		}
	}

	return FReply::Unhandled();
}

FReply SDesignSurface::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// We want to zoom into this point; i.e. keep it the same fraction offset into the panel
	const FVector2D WidgetSpaceCursorPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const int32 ZoomLevelDelta = FMath::FloorToInt(MouseEvent.GetWheelDelta());
	ChangeZoomLevel(ZoomLevelDelta, WidgetSpaceCursorPos, !bRequireControlToOverZoom || MouseEvent.IsControlDown());

	return FReply::Handled();
}

FReply SDesignSurface::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	const EGestureEvent::Type GestureType = GestureEvent.GetGestureType();
	const FVector2D& GestureDelta = GestureEvent.GetGestureDelta();
	if ( GestureType == EGestureEvent::Magnify )
	{
		TotalGestureMagnify += GestureDelta.X;
		if ( FMath::Abs(TotalGestureMagnify) > 0.07f )
		{
			// We want to zoom into this point; i.e. keep it the same fraction offset into the panel
			const FVector2D WidgetSpaceCursorPos = MyGeometry.AbsoluteToLocal(GestureEvent.GetScreenSpacePosition());
			const int32 ZoomLevelDelta = TotalGestureMagnify > 0.0f ? 1 : -1;
			ChangeZoomLevel(ZoomLevelDelta, WidgetSpaceCursorPos, !bRequireControlToOverZoom || GestureEvent.IsControlDown());
			TotalGestureMagnify = 0.0f;
		}
		return FReply::Handled();
	}
	else if ( GestureType == EGestureEvent::Scroll )
	{
		this->bIsPanning = true;
		ViewOffset -= GestureDelta / GetZoomAmount();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SDesignSurface::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	TotalGestureMagnify = 0.0f;
	return FReply::Unhandled();
}

inline float FancyMod(float Value, float Size)
{
	return ( ( Value >= 0 ) ? 0.0f : Size ) + FMath::Fmod(Value, Size);
}

float SDesignSurface::GetZoomAmount() const
{
	if ( AllowContinousZoomInterpolation.Get() )
	{
		return FMath::Lerp(ZoomLevels->GetZoomAmount(PreviousZoomLevel), ZoomLevels->GetZoomAmount(ZoomLevel), ZoomLevelGraphFade.GetLerp());
	}
	else
	{
		return ZoomLevels->GetZoomAmount(ZoomLevel);
	}
}

void SDesignSurface::ChangeZoomLevel(int32 ZoomLevelDelta, const FVector2D& WidgetSpaceZoomOrigin, bool bOverrideZoomLimiting)
{
	// We want to zoom into this point; i.e. keep it the same fraction offset into the panel
	const FVector2D PointToMaintainGraphSpace = PanelCoordToGraphCoord(WidgetSpaceZoomOrigin);

	const int32 DefaultZoomLevel = ZoomLevels->GetDefaultZoomLevel();
	const int32 NumZoomLevels = ZoomLevels->GetNumZoomLevels();

	const bool bAllowFullZoomRange =
		// To zoom in past 1:1 the user must press control
		( ZoomLevel == DefaultZoomLevel && ZoomLevelDelta > 0 && bOverrideZoomLimiting ) ||
		// If they are already zoomed in past 1:1, user may zoom freely
		( ZoomLevel > DefaultZoomLevel );

	const float OldZoomLevel = ZoomLevel;

	if ( bAllowFullZoomRange )
	{
		ZoomLevel = FMath::Clamp(ZoomLevel + ZoomLevelDelta, 0, NumZoomLevels - 1);
	}
	else
	{
		// Without control, we do not allow zooming in past 1:1.
		ZoomLevel = FMath::Clamp(ZoomLevel + ZoomLevelDelta, 0, DefaultZoomLevel);
	}

	if ( OldZoomLevel != ZoomLevel )
	{
		PostChangedZoom();

		// Note: This happens even when maxed out at a stop; so the user sees the animation and knows that they're at max zoom in/out
		ZoomLevelFade.Play( this->AsShared() );

		// Re-center the screen so that it feels like zooming around the cursor.
		{
			FSlateRect GraphBounds = ComputeSensibleBounds();

			// Make sure we are not zooming into/out into emptiness; otherwise the user will get lost..
			const FVector2D ClampedPointToMaintainGraphSpace(
				FMath::Clamp(PointToMaintainGraphSpace.X, GraphBounds.Left, GraphBounds.Right),
				FMath::Clamp(PointToMaintainGraphSpace.Y, GraphBounds.Top, GraphBounds.Bottom)
				);

			this->ViewOffset = ClampedPointToMaintainGraphSpace - WidgetSpaceZoomOrigin / GetZoomAmount();
		}
	}
}

FSlateRect SDesignSurface::ComputeSensibleBounds() const
{
	// Pad it out in every direction, to roughly account for nodes being of non-zero extent
	const float Padding = 100.0f;

	FSlateRect Bounds = ComputeAreaBounds();
	Bounds.Left -= Padding;
	Bounds.Top -= Padding;
	Bounds.Right -= Padding;
	Bounds.Bottom -= Padding;

	return Bounds;
}

void SDesignSurface::PostChangedZoom()
{
}

bool SDesignSurface::ScrollToLocation(const FGeometry& MyGeometry, FVector2D DesiredCenterPosition, const float InDeltaTime)
{
	const FVector2D HalfOFScreenInGraphSpace = 0.5f * MyGeometry.Size / GetZoomAmount();
	FVector2D CurrentPosition = ViewOffset + HalfOFScreenInGraphSpace;

	FVector2D NewPosition = FMath::Vector2DInterpTo(CurrentPosition, DesiredCenterPosition, InDeltaTime, 10.f);
	ViewOffset = NewPosition - HalfOFScreenInGraphSpace;

	// If within 1 pixel of target, stop interpolating
	return ( ( NewPosition - DesiredCenterPosition ).Size() < 1.f );
}

bool SDesignSurface::ZoomToLocation(const FVector2D& CurrentSizeWithoutZoom, const FVector2D& DesiredSize, bool bDoneScrolling)
{
	if ( bAllowContinousZoomInterpolation && ZoomLevelGraphFade.IsPlaying() )
	{
		return false;
	}

	const int32 DefaultZoomLevel = ZoomLevels->GetDefaultZoomLevel();
	const int32 NumZoomLevels = ZoomLevels->GetNumZoomLevels();
	int32 DesiredZoom = DefaultZoomLevel;

	// Find lowest zoom level that will display all nodes
	for ( int32 Zoom = 0; Zoom < DefaultZoomLevel; ++Zoom )
	{
		const FVector2D SizeWithZoom = (CurrentSizeWithoutZoom - ZoomToFitPadding) / ZoomLevels->GetZoomAmount(Zoom);
		const FVector2D LeftOverSize = SizeWithZoom - DesiredSize;

		if ( ( DesiredSize.X > SizeWithZoom.X ) || ( DesiredSize.Y > SizeWithZoom.Y ) )
		{
			// Use the previous zoom level, this one is too tight
			DesiredZoom = FMath::Max<int32>(0, Zoom - 1);
			break;
		}
	}

	if ( DesiredZoom != ZoomLevel )
	{
		if ( bAllowContinousZoomInterpolation )
		{
			// Animate to it
			PreviousZoomLevel = ZoomLevel;
			ZoomLevel = FMath::Clamp(DesiredZoom, 0, NumZoomLevels - 1);
			ZoomLevelGraphFade.Play(this->AsShared());
			return false;
		}
		else
		{
			// Do it instantly, either first or last
			if ( DesiredZoom < ZoomLevel )
			{
				// Zooming out; do it instantly
				ZoomLevel = PreviousZoomLevel = DesiredZoom;
				ZoomLevelFade.Play(this->AsShared());
			}
			else
			{
				// Zooming in; do it last
				if ( bDoneScrolling )
				{
					ZoomLevel = PreviousZoomLevel = DesiredZoom;
					ZoomLevelFade.Play(this->AsShared());
				}
			}
		}

		PostChangedZoom();
	}

	return true;
}

void SDesignSurface::ZoomToFit(bool bInstantZoom)
{
	bTeleportInsteadOfScrollingWhenZoomingToFit = bInstantZoom;
	bDeferredZoomToExtents = true;
}

FText SDesignSurface::GetZoomText() const
{
	return ZoomLevels->GetZoomText(ZoomLevel);
}

FSlateColor SDesignSurface::GetZoomTextColorAndOpacity() const
{
	return FLinearColor(1, 1, 1, 1.25f - ZoomLevelFade.GetLerp());
}

FSlateRect SDesignSurface::ComputeAreaBounds() const
{
	return FSlateRect(0, 0, 0, 0);
}

FVector2D SDesignSurface::GetViewOffset() const
{
	return ViewOffset;
}

float SDesignSurface::GetSnapGridSize() const
{
	return SnapGridSize;
}

FVector2D SDesignSurface::GraphCoordToPanelCoord(const FVector2D& GraphSpaceCoordinate) const
{
	return ( GraphSpaceCoordinate - GetViewOffset() ) * GetZoomAmount();
}

FVector2D SDesignSurface::PanelCoordToGraphCoord(const FVector2D& PanelSpaceCoordinate) const
{
	return PanelSpaceCoordinate / GetZoomAmount() + GetViewOffset();
}

void SDesignSurface::PaintBackgroundAsLines(const FSlateBrush* BackgroundImage, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32& DrawLayerId) const
{
	const bool bAntialias = false;

	const int32 RulePeriod = (int32)FEditorStyle::GetFloat("Graph.Panel.GridRulePeriod");
	check(RulePeriod > 0);

	const FLinearColor RegularColor(FEditorStyle::GetColor("Graph.Panel.GridLineColor"));
	const FLinearColor RuleColor(FEditorStyle::GetColor("Graph.Panel.GridRuleColor"));
	const FLinearColor CenterColor(FEditorStyle::GetColor("Graph.Panel.GridCenterColor"));
	const float GraphSmallestGridSize = 8.0f;
	const float RawZoomFactor = GetZoomAmount();
	const float NominalGridSize = GetSnapGridSize();

	float ZoomFactor = RawZoomFactor;
	float Inflation = 1.0f;
	while ( ZoomFactor*Inflation*NominalGridSize <= GraphSmallestGridSize )
	{
		Inflation *= 2.0f;
	}

	const float GridCellSize = NominalGridSize * ZoomFactor * Inflation;

	const float GraphSpaceGridX0 = FancyMod(ViewOffset.X, Inflation * NominalGridSize * RulePeriod);
	const float GraphSpaceGridY0 = FancyMod(ViewOffset.Y, Inflation * NominalGridSize * RulePeriod);

	float ImageOffsetX = GraphSpaceGridX0 * -ZoomFactor;
	float ImageOffsetY = GraphSpaceGridY0 * -ZoomFactor;

	const FVector2D ZeroSpace = GraphCoordToPanelCoord(FVector2D::ZeroVector);

	// Fill the background
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		DrawLayerId,
		AllottedGeometry.ToPaintGeometry(),
		BackgroundImage,
		MyClippingRect
		);

	TArray<FVector2D> LinePoints;
	new (LinePoints)FVector2D(0.0f, 0.0f);
	new (LinePoints)FVector2D(0.0f, 0.0f);

	// Horizontal bars
	for ( int32 GridIndex = 0; ImageOffsetY < AllottedGeometry.Size.Y; ImageOffsetY += GridCellSize, ++GridIndex )
	{
		if ( ImageOffsetY >= 0.0f )
		{
			const bool bIsRuleLine = ( GridIndex % RulePeriod ) == 0;
			const int32 Layer = bIsRuleLine ? ( DrawLayerId + 1 ) : DrawLayerId;

			const FLinearColor* Color = bIsRuleLine ? &RuleColor : &RegularColor;
			if ( FMath::IsNearlyEqual(ZeroSpace.Y, ImageOffsetY, 1.0f) )
			{
				Color = &CenterColor;
			}

			LinePoints[0] = FVector2D(0.0f, ImageOffsetY);
			LinePoints[1] = FVector2D(AllottedGeometry.Size.X, ImageOffsetY);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				Layer,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				MyClippingRect,
				ESlateDrawEffect::None,
				*Color,
				bAntialias);
		}
	}

	// Vertical bars
	for ( int32 GridIndex = 0; ImageOffsetX < AllottedGeometry.Size.X; ImageOffsetX += GridCellSize, ++GridIndex )
	{
		if ( ImageOffsetX >= 0.0f )
		{
			const bool bIsRuleLine = ( GridIndex % RulePeriod ) == 0;
			const int32 Layer = bIsRuleLine ? ( DrawLayerId + 1 ) : DrawLayerId;

			const FLinearColor* Color = bIsRuleLine ? &RuleColor : &RegularColor;
			if ( FMath::IsNearlyEqual(ZeroSpace.X, ImageOffsetX, 1.0f) )
			{
				Color = &CenterColor;
			}

			LinePoints[0] = FVector2D(ImageOffsetX, 0.0f);
			LinePoints[1] = FVector2D(ImageOffsetX, AllottedGeometry.Size.Y);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				Layer,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				MyClippingRect,
				ESlateDrawEffect::None,
				*Color,
				bAntialias);
		}
	}

	DrawLayerId += 2;
}

#undef LOCTEXT_NAMESPACE
