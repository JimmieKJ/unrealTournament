// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Slate/SRetainerWidget.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"


DECLARE_CYCLE_STAT(TEXT("Retainer Widget Tick"), STAT_SlateRetainerWidgetTick, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Retainer Widget Paint"), STAT_SlateRetainerWidgetPaint, STATGROUP_Slate);

#if !UE_BUILD_SHIPPING

/** True if we should allow widgets to be cached in the UI at all. */
TAutoConsoleVariable<int32> EnableRetainedRendering(
	TEXT("Slate.EnableRetainedRendering"),
	true,
	TEXT("Whether to attempt to render things in SRetainerWidgets to render targets first."));

static bool IsRetainedRenderingEnabled()
{
	return EnableRetainedRendering.GetValueOnGameThread() == 1;
}

FOnRetainedModeChanged SRetainerWidget::OnRetainerModeChangedDelegate;
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


//---------------------------------------------------

SRetainerWidget::SRetainerWidget()
	: CachedWindowToDesktopTransform(0, 0)
	, WidgetRenderer( /* bUseGammaCorrection */ true)
	, DynamicEffect(nullptr)
{
}

SRetainerWidget::~SRetainerWidget()
{
	if( FSlateApplication::IsInitialized() )
	{
		if (FApp::CanEverRender())
		{
			FSlateApplication::Get().OnPreTick().RemoveAll( this );
		}

#if !UE_BUILD_SHIPPING
		OnRetainerModeChangedDelegate.RemoveAll( this );
#endif
	}
	
}

void SRetainerWidget::Construct(const FArguments& InArgs)
{
	STAT(MyStatId = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_Slate>(InArgs._StatId);)

	RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->SRGB = true;
	RenderTarget->TargetGamma = 1;
	RenderTarget->ClearColor = FLinearColor::Transparent;

	SurfaceBrush.SetResourceObject(RenderTarget);

	Window = SNew(SVirtualWindow);
	Window->SetShouldResolveDeferred(false);
	HitTestGrid = MakeShareable(new FHittestGrid());

	WidgetRenderer.SetIsPrepassNeeded(false);

	MyWidget = InArgs._Content.Widget;
	Phase = InArgs._Phase;
	PhaseCount = InArgs._PhaseCount;

	LastDrawTime = FApp::GetCurrentTime();
	LastTickedFrame = 0;

	bRenderingOffscreenDesire = true;
	bRenderingOffscreen = false;

	Window->SetContent(MyWidget.ToSharedRef());

	ChildSlot
	[
		Window.ToSharedRef()
	];

	if ( FSlateApplication::IsInitialized() )
	{
		if (FApp::CanEverRender())
		{
			FSlateApplication::Get().OnPreTick().AddRaw(this, &SRetainerWidget::OnTickRetainers);
		}

#if !UE_BUILD_SHIPPING
		OnRetainerModeChangedDelegate.AddRaw(this, &SRetainerWidget::OnRetainerModeChanged);

		static bool bStaticInit = false;

		if ( !bStaticInit )
		{
			bStaticInit = true;
			EnableRetainedRendering.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&SRetainerWidget::OnRetainerModeCVarChanged));
		}
#endif
	}
}

bool SRetainerWidget::ShouldBeRenderingOffscreen() const
{
	return bRenderingOffscreenDesire && IsRetainedRenderingEnabled();
}

bool SRetainerWidget::IsAnythingVisibleToRender() const
{
	return GetVisibility().IsVisible() && MyWidget.IsValid() && MyWidget->GetVisibility().IsVisible();
}

void SRetainerWidget::OnRetainerModeChanged()
{
	RefreshRenderingMode();
	Invalidate(EInvalidateWidget::Layout);
}

#if !UE_BUILD_SHIPPING

void SRetainerWidget::OnRetainerModeCVarChanged( IConsoleVariable* CVar )
{
	OnRetainerModeChangedDelegate.Broadcast();
}

#endif

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

		Window->SetContent(MyWidget.ToSharedRef());
	}
}

void SRetainerWidget::SetContent(const TSharedRef< SWidget >& InContent)
{
	MyWidget = InContent;
	Window->SetContent(InContent);
}

UMaterialInstanceDynamic* SRetainerWidget::GetEffectMaterial() const
{
	return DynamicEffect;
}

void SRetainerWidget::SetEffectMaterial(UMaterialInterface* EffectMaterial)
{
	if ( EffectMaterial )
	{
		DynamicEffect = Cast<UMaterialInstanceDynamic>(EffectMaterial);
		if ( !DynamicEffect )
		{
			DynamicEffect = UMaterialInstanceDynamic::Create(EffectMaterial, GetTransientPackage());
		}

		SurfaceBrush.SetResourceObject(DynamicEffect);
	}
	else
	{
		DynamicEffect = nullptr;
		SurfaceBrush.SetResourceObject(RenderTarget);
	}
}

void SRetainerWidget::SetTextureParameter(FName TextureParameter)
{
	DynamicEffectTextureParameter = TextureParameter;
}

void SRetainerWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(RenderTarget);
	Collector.AddReferencedObject(DynamicEffect);
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

void SRetainerWidget::OnTickRetainers(float DeltaTime)
{
	STAT(FScopeCycleCounter TickCycleCounter(MyStatId);)

	// we should not be added to tick if we're not rendering
	checkSlow(FApp::CanEverRender());

	const bool bShouldRenderAnything = IsAnythingVisibleToRender();
	if ( bRenderingOffscreen && bShouldRenderAnything )
	{
		SCOPE_CYCLE_COUNTER( STAT_SlateRetainerWidgetTick );
		if ( LastTickedFrame != GFrameCounter && ( GFrameCounter % PhaseCount ) == Phase )
		{
			LastTickedFrame = GFrameCounter;
			double TimeSinceLastDraw = FApp::GetCurrentTime() - LastDrawTime;

			FPaintGeometry PaintGeometry = CachedAllottedGeometry.ToPaintGeometry();

			//const FSlateRenderTransform& RenderTransform = PaintGeometry.GetAccumulatedRenderTransform()

			// extract the layout transform from the draw element
			FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(PaintGeometry.DrawScale, PaintGeometry.DrawPosition)));
			// The clip rect is NOT subject to the rotations specified by MakeRotatedBox.
			FSlateRotatedClipRectType RenderClipRect = FSlateRotatedClipRectType::MakeSnappedRotatedRect(CachedClippingRect, InverseLayoutTransform, CachedAllottedGeometry.GetAccumulatedRenderTransform());

			//const FVector2D ScaledSize = PaintGeometry.GetLocalSize() * PaintGeometry.DrawScale;
			//const uint32 RenderTargetWidth  = FMath::RoundToInt(ScaledSize.X);
			//const uint32 RenderTargetHeight = FMath::RoundToInt(ScaledSize.Y);

			float OffsetX = PaintGeometry.DrawPosition.X - ( (int32)PaintGeometry.DrawPosition.X );
			float OffsetY = PaintGeometry.DrawPosition.Y - ( (int32)PaintGeometry.DrawPosition.Y );

			const uint32 RenderTargetWidth  = FMath::RoundToInt(RenderClipRect.ExtentX.X);
			const uint32 RenderTargetHeight = FMath::RoundToInt(RenderClipRect.ExtentY.Y);

			FVector2D ViewOffset = FVector2D(FMath::RoundToInt(PaintGeometry.DrawPosition.X), FMath::RoundToInt(PaintGeometry.DrawPosition.Y));

			// Keep the visibilities the same, the proxy window should maintain the same visible/non-visible hit-testing of the retainer.
			Window->SetVisibility(GetVisibility());

			// Need to prepass.
			Window->SlatePrepass(CachedAllottedGeometry.Scale);

			if ( RenderTargetWidth != 0 && RenderTargetHeight != 0 )
			{
				if ( MyWidget->GetVisibility().IsVisible() )
				{
					if ( RenderTarget->GetSurfaceWidth() != RenderTargetWidth ||
						 RenderTarget->GetSurfaceHeight() != RenderTargetHeight )
					{
						const bool bForceLinearGamma = false;
						RenderTarget->InitCustomFormat(RenderTargetWidth, RenderTargetHeight, PF_B8G8R8A8, bForceLinearGamma);
						RenderTarget->UpdateResourceImmediate();
					}

					// TODO Need to improve widget renderer to allow us to not render deferred rendered widgets into the render target, as they will likely be clipped.

					const float Scale = CachedAllottedGeometry.Scale;
					//const FVector2D DrawSize = FVector2D(RenderTargetWidth, RenderTargetHeight);
					//FSlateRect ClipRect = CachedClippingRect.OffsetBy(-PaintGeometry.DrawPosition);
					//ClipRect.Left *= Scale;
					//ClipRect.Right *= Scale;
					//ClipRect.Top *= Scale;
					//ClipRect.Bottom *= Scale;

					const FVector2D DrawSize = FVector2D(RenderTargetWidth, RenderTargetHeight);
					//const FVector2D DrawSize = PaintGeometry.GetLocalSize();
					const FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize * ( 1 / Scale ), FSlateLayoutTransform(Scale, PaintGeometry.DrawPosition));

					// Update the surface brush to match the latest size.
					SurfaceBrush.ImageSize = DrawSize;

					WidgetRenderer.ViewOffset = -ViewOffset;

					WidgetRenderer.DrawWindow(
						RenderTarget,
						HitTestGrid.ToSharedRef(),
						Window.ToSharedRef(),
						WindowGeometry,
						WindowGeometry.GetClippingRect(),
						TimeSinceLastDraw);
				}
			}

			LastDrawTime = FApp::GetCurrentTime();
		}
	}
}

int32 SRetainerWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	STAT(FScopeCycleCounter PaintCycleCounter(MyStatId);)

	SRetainerWidget* MutableThis = const_cast<SRetainerWidget*>( this );

	MutableThis->RefreshRenderingMode();

	const bool bShouldRenderAnything = IsAnythingVisibleToRender();
	if ( bRenderingOffscreen && bShouldRenderAnything )
	{
		SCOPE_CYCLE_COUNTER( STAT_SlateRetainerWidgetPaint );
		CachedAllottedGeometry = AllottedGeometry;
		CachedWindowToDesktopTransform = Args.GetWindowToDesktopTransform();
		CachedClippingRect = MyClippingRect;

		if ( RenderTarget->GetSurfaceWidth() >= 1 && RenderTarget->GetSurfaceHeight() >= 1 )
		{
			const FLinearColor ComputedColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get() * SurfaceBrush.GetTint(InWidgetStyle));
			// Retainer widget uses premultiplied alpha, so premultiply the color by the alpha to respect opactiy.
			const FLinearColor PremultipliedColorAndOpacity(ComputedColorAndOpacity*ComputedColorAndOpacity.A);

			if ( DynamicEffect )
			{
				DynamicEffect->SetTextureParameterValue(DynamicEffectTextureParameter, RenderTarget);
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				&SurfaceBrush,
				MyClippingRect,
				ESlateDrawEffect::NoGamma | ESlateDrawEffect::PreMultipliedAlpha,
				FLinearColor(PremultipliedColorAndOpacity.R, PremultipliedColorAndOpacity.G, PremultipliedColorAndOpacity.B, PremultipliedColorAndOpacity.A)
				);
			
			TSharedRef<SRetainerWidget> SharedMutableThis = SharedThis(MutableThis);
			Args.InsertCustomHitTestPath(SharedMutableThis, Args.GetLastHitTestIndex());

			for ( auto& DeferredPaint : WidgetRenderer.DeferredPaints )
			{
				OutDrawElements.QueueDeferredPainting(DeferredPaint->Copy(Args));
			}
		}
		
		return LayerId;
	}
	else if( bShouldRenderAnything )
	{
		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	return LayerId;
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
	const FVector2D LocalPosition = DesktopSpaceCoordinate - CachedWindowToDesktopTransform;// InGeometry.AbsoluteToLocal(DesktopSpaceCoordinate) * InGeometry.Scale;
	const FVector2D LastLocalPosition = DesktopSpaceCoordinate - CachedWindowToDesktopTransform;// InGeometry.AbsoluteToLocal(DesktopSpaceCoordinate) * InGeometry.Scale;

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
	ArrangedChildren.AddWidget(FArrangedWidget(MyWidget.ToSharedRef(), CachedAllottedGeometry));
}

TSharedPtr<struct FVirtualPointerPosition> SRetainerWidget::TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& InGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const
{
	//const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(ScreenSpaceMouseCoordinate);
	//const FVector2D LastLocalPosition = InGeometry.AbsoluteToLocal(LastScreenSpaceMouseCoordinate);

	return MakeShareable(new FVirtualPointerPosition(ScreenSpaceMouseCoordinate, LastScreenSpaceMouseCoordinate));
}
