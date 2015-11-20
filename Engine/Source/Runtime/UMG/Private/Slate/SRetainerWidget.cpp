// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "Slate/SlateTextures.h"
#include "SRetainerWidget.h"
#include "Runtime/SlateRHIRenderer/Public/Interfaces/ISlateRHIRendererModule.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

/** True if we should allow widgets to be cached in the UI at all. */
TAutoConsoleVariable<int32> EnableRetainedRendering(
	TEXT("Slate.EnableRetainedRendering"),
	true,
	TEXT("Whether to attempt to render things in SRetainerWidgets to render targets first."));

static bool IsRetainedRenderingEnabled()
{
	return EnableRetainedRendering.GetValueOnGameThread() == 1;
}

#else

static bool IsRetainedRenderingEnabled()
{
	return true;
}

#endif



static FVector2D RoundToInt(const FVector2D& Vec)
{
	return FVector2D(FMath::RoundToInt(Vec.X), FMath::RoundToInt(Vec.Y));
}

/**
* Used to construct a rotated rect from an aligned clip rect and a set of layout and render transforms from the geometry, snapped to pixel boundaries. Returns a float or float16 version of the rect based on the typedef.
*/
static FSlateRotatedClipRectType ToSnappedRotatedRect(const FSlateRect& ClipRectInLayoutWindowSpace, const FSlateLayoutTransform& InverseLayoutTransform, const FSlateRenderTransform& RenderTransform)
{
	FSlateRotatedRect RotatedRect = TransformRect(
		Concatenate(InverseLayoutTransform, RenderTransform),
		FSlateRotatedRect(ClipRectInLayoutWindowSpace));

	// Pixel snapping is done here by rounding the resulting floats to ints, we do this before
	// calculating the final extents of the clip box otherwise we'll get a smaller clip rect than a visual
	// rect where each point is individually snapped.
	FVector2D SnappedTopLeft = RoundToInt(RotatedRect.TopLeft);
	FVector2D SnappedTopRight = RoundToInt(RotatedRect.TopLeft + RotatedRect.ExtentX);
	FVector2D SnappedBottomLeft = RoundToInt(RotatedRect.TopLeft + RotatedRect.ExtentY);

	//NOTE: We explicitly do not re-snap the extent x/y, it wouldn't be correct to snap again in distance space
	// even if two points are snapped, their distance wont necessarily be a whole number if those points are not
	// axis aligned.
	return FSlateRotatedClipRectType(
		SnappedTopLeft,
		SnappedTopRight - SnappedTopLeft,
		SnappedBottomLeft - SnappedTopLeft);
}




//---------------------------------------------------

SRetainerWidget::SRetainerWidget()
	: WidgetRenderer( /* bUseGammaCorrection */ true)
{
}

SRetainerWidget::~SRetainerWidget()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(DeleteShaderResource,
		FSlateRenderTargetRHI*, InRenderTargetRHI, RenderTargetResource,
		{
			delete InRenderTargetRHI;
		});
}

void SRetainerWidget::Construct(const FArguments& InArgs)
{
	RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->SRGB = true;
	RenderTarget->TargetGamma = 1;
	RenderTarget->ClearColor = FLinearColor::Transparent;

	RenderTargetBrush.SetResourceObject(RenderTarget);

	Window = SNew(SVirtualWindow);
	HitTestGrid = MakeShareable(new FHittestGrid());

	WidgetRenderer.SetIsPrepassNeeded(false);

	RenderTargetResource = new FSlateRenderTargetRHI(nullptr, 0, 0);

	MyWidget = InArgs._Content.Widget;
	Phase = InArgs._Phase;
	PhaseCount = InArgs._PhaseCount;

	LastDrawTime = FApp::GetCurrentTime();
	LastTickedFrame = 0;

	bRenderingOffscreenDesire = true;
	bRenderingOffscreen = false;

	ChildSlot
	[
		MyWidget.ToSharedRef()
	];

	Window->SetContent(MyWidget.ToSharedRef());
}

FIntPoint SRetainerWidget::GetSize() const
{
	FIntPoint Point;
	Point.X = (int32)RenderTarget->GetSurfaceWidth();
	Point.Y = (int32)RenderTarget->GetSurfaceHeight();

	return Point;
}

bool SRetainerWidget::RequiresVsync() const
{
	return false;
}

FSlateShaderResource* SRetainerWidget::GetViewportRenderTargetTexture() const
{
	return RenderTargetResource;
}

bool SRetainerWidget::IsViewportTextureAlphaOnly() const
{
	return false;
}

bool SRetainerWidget::ShouldBeRenderingOffscreen() const
{
	return bRenderingOffscreenDesire && IsRetainedRenderingEnabled();
}

void SRetainerWidget::SetRetainedRendering(bool bRetainRendering)
{
	bRenderingOffscreenDesire = bRetainRendering;
}

void SRetainerWidget::RefreshRenderingMode()
{
	const bool bShouldBeRenderingOffscreen = ShouldBeRenderingOffscreen();

	if ( bRenderingOffscreen != bShouldBeRenderingOffscreen )
	{
		bRenderingOffscreen = bShouldBeRenderingOffscreen;

		ChildSlot
		[
			MyWidget.ToSharedRef()
		];

		// TODO When parent pointers come into play, we need to make it so the virtual window,
		// doesn't actually ever own the content, it just sorta kind of owns it.
		if ( bRenderingOffscreen )
		{
			Window->SetContent(MyWidget.ToSharedRef());
		}
		else
		{
			Window->SetContent(SNullWidget::NullWidget);
		}
	}
}

void SRetainerWidget::SetContent(const TSharedRef< SWidget >& InContent)
{
	MyWidget = InContent;

	ChildSlot
	[
		InContent
	];

	if ( bRenderingOffscreen )
	{
		Window->SetContent(InContent);
	}
}

void SRetainerWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(RenderTarget);
}

FChildren* SRetainerWidget::GetChildren()
{
	if ( bRenderingOffscreen )
	{
		return &EmptyChildSlot;
	}
	else
	{
		return SCompoundWidget::GetChildren();
	}
}

bool SRetainerWidget::ComputeVolatility() const
{
	return true;
}

void SRetainerWidget::Tick(float DeltaTime)
{
	if ( bRenderingOffscreen )
	{
		if ( LastTickedFrame != GFrameCounter && ( GFrameCounter % PhaseCount ) == Phase )
		{
			LastTickedFrame = GFrameCounter;
			double TimeSinceLastDraw = FApp::GetCurrentTime() - LastDrawTime;

			FPaintGeometry PaintGeometry = CachedAllottedGeometry.ToPaintGeometry();

			// extract the layout transform from the draw element
			FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(PaintGeometry.DrawScale, PaintGeometry.DrawPosition)));
			// The clip rect is NOT subject to the rotations specified by MakeRotatedBox.
			FSlateRotatedClipRectType RenderClipRect = ToSnappedRotatedRect(CachedClippingRect, InverseLayoutTransform, CachedAllottedGeometry.GetAccumulatedRenderTransform());

			const uint32 RenderTargetWidth  = FMath::RoundToInt(RenderClipRect.ExtentX.X);
			const uint32 RenderTargetHeight = FMath::RoundToInt(RenderClipRect.ExtentY.Y);

			// Need to prepass.
			Window->SlatePrepass(CachedAllottedGeometry.Scale);

			if ( RenderTargetWidth != 0 && RenderTargetHeight != 0 )
			{
				if ( MyWidget->GetVisibility().IsVisible() )
				{
					if ( RenderTargetResource->GetWidth() != RenderTargetWidth ||
						RenderTargetResource->GetHeight() != RenderTargetHeight )
					{
						const bool bForceLinearGamma = false;
						RenderTarget->InitCustomFormat(RenderTargetWidth, RenderTargetHeight, PF_B8G8R8A8, bForceLinearGamma);

						ENQUEUE_UNIQUE_RENDER_COMMAND_FOURPARAMETER(UpdateShaderResource,
							UTextureRenderTarget2D*, InRenderTarget, RenderTarget,
							FSlateRenderTargetRHI*, InRenderTargetRHI, RenderTargetResource,
							uint32, InRenderTargetWidth, RenderTargetWidth,
							uint32, InRenderTargetHeight, RenderTargetHeight,
							{
								FTextureRenderTarget2DResource* CurrentResource = static_cast<FTextureRenderTarget2DResource*>( InRenderTarget->GetRenderTargetResource() );
								InRenderTargetRHI->SetRHIRef(CurrentResource->GetTextureRHI(), InRenderTargetWidth, InRenderTargetHeight);
							});
					}

					// TODO Need to improve widget renderer to allow us to not render deferred rendered widgets into the render target, as they will likely be clipped.

					WidgetRenderer.DrawWindow(
						RenderTarget,
						HitTestGrid.ToSharedRef(),
						Window.ToSharedRef(),
						CachedAllottedGeometry.Scale,
						FVector2D(RenderTargetWidth, RenderTargetHeight),
						TimeSinceLastDraw);
				}
			}

			LastDrawTime = FApp::GetCurrentTime();
		}
	}
}

int32 SRetainerWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	SRetainerWidget* MutableThis = const_cast<SRetainerWidget*>( this );

	MutableThis->RefreshRenderingMode();

	if ( bRenderingOffscreen )
	{
		CachedAllottedGeometry = AllottedGeometry;
		CachedClippingRect = MyClippingRect;

		if ( RenderTarget->GetSurfaceWidth() >= 1 && RenderTarget->GetSurfaceHeight() >= 1 )
		{
			const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get() * RenderTargetBrush.GetTint(InWidgetStyle));

			const bool bEnableGammaCorrection = false;
			const bool bEnableBlending = true;

			if ( RenderTargetResource->GetWidth() != 0 && RenderTargetResource->GetWidth() != 0 )
			{
				FSlateDrawElement::MakeViewport(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					SharedThis(this),
					MyClippingRect,
					bEnableGammaCorrection,
					bEnableBlending,
					ESlateDrawEffect::PreMultipliedAlpha,
					FinalColorAndOpacity);
			}
			
			TSharedRef<SRetainerWidget> SharedMutableThis = SharedThis(MutableThis);
			Args.InsertCustomHitTestPath(SharedMutableThis, Args.GetLastHitTestIndex());
		}
		
		return LayerId;
	}
	else
	{
		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
}

FVector2D SRetainerWidget::ComputeDesiredSize(float LayoutScaleMuliplier) const
{
	if ( bRenderingOffscreen )
	{
		return MyWidget->GetDesiredSize();
	}
	else
	{
		return SCompoundWidget::ComputeDesiredSize(LayoutScaleMuliplier);
	}
}

TArray<FWidgetAndPointer> SRetainerWidget::GetBubblePathAndVirtualCursors(const FGeometry& InGeometry, FVector2D DesktopSpaceCoordinate, bool bIgnoreEnabledStatus) const
{
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(DesktopSpaceCoordinate);
	const FVector2D LastLocalPosition = InGeometry.AbsoluteToLocal(DesktopSpaceCoordinate);

	TSharedRef<FVirtualPointerPosition> VirtualMouseCoordinate = MakeShareable(new FVirtualPointerPosition(LocalPosition, LastLocalPosition));

	// TODO Where should this come from?
	const float CursorRadius = 0;

	TArray<FWidgetAndPointer> ArrangedWidgets = 
		HitTestGrid->GetBubblePath(LocalPosition, CursorRadius, bIgnoreEnabledStatus);

	for ( FWidgetAndPointer& ArrangedWidget : ArrangedWidgets )
	{
		ArrangedWidget.PointerPosition = VirtualMouseCoordinate;
	}

	return ArrangedWidgets;
}

void SRetainerWidget::ArrangeChildren(FArrangedChildren& ArrangedChildren) const
{
	ArrangedChildren.AddWidget(
		FArrangedWidget(MyWidget.ToSharedRef(), CachedAllottedGeometry));
}

TSharedPtr<struct FVirtualPointerPosition> SRetainerWidget::TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& InGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const
{
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(ScreenSpaceMouseCoordinate);
	const FVector2D LastLocalPosition = InGeometry.AbsoluteToLocal(LastScreenSpaceMouseCoordinate);

	return MakeShareable(new FVirtualPointerPosition(LocalPosition, LastLocalPosition));
}
