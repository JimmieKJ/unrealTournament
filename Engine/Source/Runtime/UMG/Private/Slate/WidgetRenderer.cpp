// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	Renderer = FModuleManager::Get().LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlate3DRenderer(bUseGammaSpace);
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
	UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, bUseGammaSpace);

	DrawWidget(RenderTarget, Widget, DrawSize, 0);

	return RenderTarget;
}

UTextureRenderTarget2D* FWidgetRenderer::CreateTargetFor(FVector2D DrawSize, TextureFilter InFilter, bool bUseGammaCorrection)
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->Filter = InFilter;
	RenderTarget->ClearColor = FLinearColor::Transparent;
	const bool bIsLinearSpace = !bUseGammaCorrection;
	RenderTarget->InitCustomFormat(DrawSize.X, DrawSize.Y, PF_B8G8R8A8, bIsLinearSpace);
	return RenderTarget;
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
#if !UE_SERVER
	FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize * (1 / Scale), FSlateLayoutTransform(Scale));

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
		Window->SlatePrepass(Scale);
	}

	// Prepare the test grid 
	HitTestGrid->ClearGridForNewFrame(WindowGeometry.GetClippingRect());

	// Get the free buffer & add our virtual window
	FSlateDrawBuffer& DrawBuffer = Renderer->GetDrawBuffer();
	FSlateWindowElementList& WindowElementList = DrawBuffer.AddWindowElementList(Window);

	int32 MaxLayerId = 0;
	{
		// Paint the window
		MaxLayerId = Window->Paint(
			FPaintArgs(Window.Get(), HitTestGrid.Get(), FVector2D::ZeroVector, FApp::GetCurrentTime(), DeltaTime),
			WindowGeometry, WindowGeometry.GetClippingRect(),
			WindowElementList,
			0,
			FWidgetStyle(),
			Window->IsEnabled());
	}

	Renderer->DrawWindow_GameThread(DrawBuffer);

	// Enqueue a command to unlock the draw buffer after all windows have been drawn
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(UWidgetComponentRenderToTexture,
		FSlateDrawBuffer&, InDrawBuffer, DrawBuffer,
		UTextureRenderTarget2D*, InRenderTarget, RenderTarget,
		ISlate3DRenderer*, InRenderer, Renderer.Get(),
		{
			InRenderer->DrawWindowToTarget_RenderThread(RHICmdList, InRenderTarget, InDrawBuffer);
		});
#endif // !UE_SERVER
}
