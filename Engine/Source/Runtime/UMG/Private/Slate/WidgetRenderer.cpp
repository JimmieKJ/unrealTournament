// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "WidgetRenderer.h"
#include "HittestGrid.h"

#if !UE_SERVER
#include "ISlateRHIRendererModule.h"
#include "ISlate3DRenderer.h"
#endif // !UE_SERVER

#include "Widgets/LayerManager/STooltipPresenter.h"
#include "Widgets/Layout/SPopup.h"

extern SLATECORE_API int32 bFoldTick;

void SVirtualWindow::Construct(const FArguments& InArgs)
{
	bIsPopupWindow = true;
	bVirtualWindow = true;
	bIsFocusable = false;
	bShouldResolveDeferred = true;
	SetCachedSize(InArgs._Size);
	SetNativeWindow(MakeShareable(new FGenericWindow()));

	ConstructWindowInternals();

	WindowOverlay->AddSlot()
	[
		SNew(SPopup)
		[
			SAssignNew(TooltipPresenter, STooltipPresenter)
		]
	];

	SetContent(SNullWidget::NullWidget);
}

FPopupMethodReply SVirtualWindow::OnQueryPopupMethod() const
{
	return FPopupMethodReply::UseMethod(EPopupMethod::UseCurrentWindow)
		.SetShouldThrottle(EShouldThrottle::No);
}

bool SVirtualWindow::OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent)
{
	TooltipPresenter->SetContent(TooltipContent.IsValid() ? TooltipContent.ToSharedRef() : SNullWidget::NullWidget);

	return true;
}

void SVirtualWindow::SetShouldResolveDeferred(bool bResolve)
{
	bShouldResolveDeferred = bResolve;
}

void SVirtualWindow::SetIsFocusable(bool bFocusable)
{
	bIsFocusable = bFocusable;
}

bool SVirtualWindow::SupportsKeyboardFocus() const
{
	return bIsFocusable;
}

int32 SVirtualWindow::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if ( bShouldResolveDeferred )
	{
		OutDrawElements.BeginDeferredGroup();
	}
	
	// We intentionally skip SWindow's OnPaint, since we are going to do our own handling of deferred groups.
	int32 MaxLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if ( bShouldResolveDeferred )
	{
		OutDrawElements.EndDeferredGroup();
	}

	return MaxLayer;
}

void SVirtualWindow::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	SWindow::OnArrangeChildren(AllottedGeometry, ArrangedChildren);

	// @HACK VREDITOR - otherwise popup layers don't work in nested child windows, in tab managers and such.
	if ( ArrangedChildren.Allows3DWidgets() )
	{
		const TArray< TSharedRef<SWindow> >& WindowChildren = GetChildWindows();
		for ( int32 ChildIndex=0; ChildIndex < WindowChildren.Num(); ++ChildIndex )
		{
			const TSharedRef<SWindow>& ChildWindow = WindowChildren[ChildIndex];
			FGeometry ChildWindowGeometry = ChildWindow->GetWindowGeometryInWindow();
			ChildWindow->ArrangeChildren(ChildWindowGeometry, ArrangedChildren);
		}
	}
}

FWidgetRenderer::FWidgetRenderer(bool bUseGammaCorrection, bool bInClearTarget)
	: bPrepassNeeded(true)
	, bUseGammaSpace(bUseGammaCorrection)
	, bClearTarget(bInClearTarget)
	, ViewOffset(0.0f, 0.0f)
{
#if !UE_SERVER
	if ( LIKELY(FApp::CanEverRender()) )
	{
		Renderer = FModuleManager::Get().LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlate3DRenderer(bUseGammaSpace);
	}
#endif
}

FWidgetRenderer::~FWidgetRenderer()
{
}

ISlate3DRenderer* FWidgetRenderer::GetSlateRenderer()
{
	return Renderer.Get();
}

UTextureRenderTarget2D* FWidgetRenderer::DrawWidget(const TSharedRef<SWidget>& Widget, FVector2D DrawSize)
{
	if ( LIKELY(FApp::CanEverRender()) )
	{
		UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, bUseGammaSpace);

		DrawWidget(RenderTarget, Widget, DrawSize, 0);

		return RenderTarget;
	}

	return nullptr;
}

UTextureRenderTarget2D* FWidgetRenderer::CreateTargetFor(FVector2D DrawSize, TextureFilter InFilter, bool bUseGammaCorrection)
{
	if ( LIKELY(FApp::CanEverRender()) )
	{
		const bool bIsLinearSpace = !bUseGammaCorrection;

		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
		RenderTarget->Filter = InFilter;
		RenderTarget->ClearColor = FLinearColor::Transparent;
		RenderTarget->SRGB = bIsLinearSpace;
		RenderTarget->TargetGamma = 1;
		RenderTarget->InitCustomFormat(DrawSize.X, DrawSize.Y, PF_B8G8R8A8, bIsLinearSpace);
		RenderTarget->UpdateResourceImmediate(true);

		return RenderTarget;
	}

	return nullptr;
}

void FWidgetRenderer::DrawWidget(UTextureRenderTarget2D* RenderTarget, const TSharedRef<SWidget>& Widget, FVector2D DrawSize, float DeltaTime)
{
	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(DrawSize);
	TSharedRef<FHittestGrid> HitTestGrid = MakeShareable(new FHittestGrid());

	Window->SetContent(Widget);
	Window->Resize(DrawSize);

	DrawWindow(RenderTarget, HitTestGrid, Window, 1, DrawSize, DeltaTime);
}

void FWidgetRenderer::DrawWindow(
	UTextureRenderTarget2D* RenderTarget,
	TSharedRef<FHittestGrid> HitTestGrid,
	TSharedRef<SWindow> Window,
	float Scale,
	FVector2D DrawSize,
	float DeltaTime)
{
	FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize * ( 1 / Scale ), FSlateLayoutTransform(Scale));

	DrawWindow(
		RenderTarget,
		HitTestGrid,
		Window,
		WindowGeometry,
		WindowGeometry.GetClippingRect(),
		DeltaTime
		);
}

void FWidgetRenderer::DrawWindow(
	UTextureRenderTarget2D* RenderTarget,
	TSharedRef<FHittestGrid> HitTestGrid,
	TSharedRef<SWindow> Window,
	FGeometry WindowGeometry,
	FSlateRect WindowClipRect,
	float DeltaTime)
{
#if !UE_SERVER
	if ( LIKELY(FApp::CanEverRender()) )
	{
	    if ( !bFoldTick )
	    {
		    Window->TickWidgetsRecursively(WindowGeometry, FApp::GetCurrentTime(), DeltaTime);
	    }
    
	    if ( bPrepassNeeded )
	    {
		    // Ticking can cause geometry changes.  Recompute
		    Window->SlatePrepass(WindowGeometry.Scale);
	    }
    
	    // Prepare the test grid 
	    HitTestGrid->ClearGridForNewFrame(WindowClipRect);
    
	    // Get the free buffer & add our virtual window
	    FSlateDrawBuffer& DrawBuffer = Renderer->GetDrawBuffer();
	    FSlateWindowElementList& WindowElementList = DrawBuffer.AddWindowElementList(Window);
    
	    int32 MaxLayerId = 0;
	    {
		    FPaintArgs PaintArgs(Window.Get(), HitTestGrid.Get(), FVector2D::ZeroVector, FApp::GetCurrentTime(), DeltaTime);
    
		    // Paint the window
		    MaxLayerId = Window->Paint(
			    PaintArgs,
			    WindowGeometry, WindowClipRect,
			    WindowElementList,
			    0,
			    FWidgetStyle(),
			    Window->IsEnabled());
	    }

		//MaxLayerId = WindowElementList.PaintDeferred(MaxLayerId);
		DeferredPaints = WindowElementList.GetDeferredPaintList();

		Renderer->DrawWindow_GameThread(DrawBuffer);

		DrawBuffer.ViewOffset = ViewOffset;

		struct FRenderThreadContext
		{
			FSlateDrawBuffer* DrawBuffer;
			FTextureRenderTarget2DResource* RenderTargetResource;
			TSharedPtr<ISlate3DRenderer, ESPMode::ThreadSafe> Renderer;
			bool bClearTarget;
		}
		Context =		
		{
			&DrawBuffer,
			static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource()),
			Renderer,
			bClearTarget
		};

		// Enqueue a command to unlock the draw buffer after all windows have been drawn
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(FWidgetRenderer_DrawWindow,
			FRenderThreadContext, InContext, Context,
			{
				InContext.Renderer->DrawWindowToTarget_RenderThread(RHICmdList, InContext.RenderTargetResource, *InContext.DrawBuffer, InContext.bClearTarget);
			});
	}
#endif // !UE_SERVER
}

void FWidgetRenderer::PrepassWindowAndChildren(TSharedRef<SWindow> Window, float Scale)
{
	Window->SlatePrepass(Scale);

	const TArray< TSharedRef<SWindow> >& WindowChildren = Window->GetChildWindows();
	for ( int32 ChildIndex=0; ChildIndex < WindowChildren.Num(); ++ChildIndex )
	{
		const TSharedRef<SWindow>& ChildWindow = WindowChildren[ChildIndex];
		PrepassWindowAndChildren(ChildWindow, Scale);
	}
}

void FWidgetRenderer::DrawWindowAndChildren(
	UTextureRenderTarget2D* RenderTarget,
	FSlateDrawBuffer& DrawBuffer,
	TSharedRef<FHittestGrid> HitTestGrid,
	TSharedRef<SWindow> Window,
	FGeometry WindowGeometry,
	FSlateRect WindowClipRect,
	float DeltaTime)
{
	if ( !bFoldTick )
	{
		Window->TickWidgetsRecursively(WindowGeometry, FApp::GetCurrentTime(), DeltaTime);
	}

	// Prepare the test grid 
	HitTestGrid->ClearGridForNewFrame(WindowClipRect);

	// Get the free buffer & add our virtual window
	FSlateWindowElementList& WindowElementList = DrawBuffer.AddWindowElementList(Window);

	int32 MaxLayerId = 0;
	{
		FPaintArgs PaintArgs(Window.Get(), HitTestGrid.Get(), FVector2D::ZeroVector, FApp::GetCurrentTime(), DeltaTime);

		// Paint the window
		MaxLayerId = Window->Paint(
			PaintArgs,
			WindowGeometry, WindowClipRect,
			WindowElementList,
			0,
			FWidgetStyle(),
			Window->IsEnabled());
	}

	// Draw the child windows
	const TArray< TSharedRef<SWindow> >& WindowChildren = Window->GetChildWindows();
	for ( int32 ChildIndex=0; ChildIndex < WindowChildren.Num(); ++ChildIndex )
	{
		const TSharedRef<SWindow>& ChildWindow = WindowChildren[ChildIndex];
		FGeometry ChildWindowGeometry = ChildWindow->GetWindowGeometryInWindow();
		FSlateRect ChildWindowClipRect = ChildWindow->GetClippingRectangleInWindow();
		DrawWindowAndChildren(RenderTarget, DrawBuffer, HitTestGrid, ChildWindow, ChildWindowGeometry, ChildWindowClipRect, DeltaTime);
	}
}
