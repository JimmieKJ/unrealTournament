// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "WidgetRenderer.h"
#include "HittestGrid.h"

#if !UE_SERVER
#include "ISlateRHIRendererModule.h"
#include "ISlate3DRenderer.h"
#endif // !UE_SERVER

extern SLATECORE_API int32 bFoldTick;

void SVirtualWindow::Construct(const FArguments& InArgs)
{
	bIsPopupWindow = true;
	SetCachedSize(InArgs._Size);
	SetNativeWindow(MakeShareable(new FGenericWindow()));
	SetContent(SNullWidget::NullWidget);
}

FWidgetRenderer::FWidgetRenderer(bool bUseGammaCorrection)
	: bPrepassNeeded(true)
	, bUseGammaSpace(bUseGammaCorrection)
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
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
	if ( !IsRunningDedicatedServer() )
	{
		UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, bUseGammaSpace);

		DrawWidget(RenderTarget, Widget, DrawSize, 0);

		return RenderTarget;
	}

	return nullptr;
}

UTextureRenderTarget2D* FWidgetRenderer::CreateTargetFor(FVector2D DrawSize, TextureFilter InFilter, bool bUseGammaCorrection)
{
	if ( !IsRunningDedicatedServer() )
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
	if (!IsRunningDedicatedServer())
	{
		if ( GUsingNullRHI )
		{
			return;
		}
	
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

		Renderer->DrawWindow_GameThread(DrawBuffer);

		struct FRenderThreadContext
		{
			FSlateDrawBuffer* DrawBuffer;
			FTextureRenderTarget2DResource* RenderTargetResource;
			TSharedPtr<ISlate3DRenderer, ESPMode::ThreadSafe> Renderer;
		};
		FRenderThreadContext Context =
		{
			&DrawBuffer,
			static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource()),
			Renderer
		};

		// Enqueue a command to unlock the draw buffer after all windows have been drawn
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(FWidgetRenderer_DrawWindow,
			FRenderThreadContext, InContext, Context,
			{
				InContext.Renderer->DrawWindowToTarget_RenderThread(RHICmdList, InContext.RenderTargetResource, *InContext.DrawBuffer);
			});
	}
#endif // !UE_SERVER
}
