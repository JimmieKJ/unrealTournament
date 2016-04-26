// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "Slate3DRenderer.h"
#include "ElementBatcher.h"

FSlate3DRenderer::FSlate3DRenderer( TSharedRef<FSlateFontServices> InSlateFontServices, TSharedRef<FSlateRHIResourceManager> InResourceManager, bool bUseGammaCorrection )
	: SlateFontServices( InSlateFontServices )
	, ResourceManager( InResourceManager )
{
	const int32 InitialBufferSize = 200;
	RenderTargetPolicy = MakeShareable( new FSlateRHIRenderingPolicy( SlateFontServices, ResourceManager, InitialBufferSize ) );
	RenderTargetPolicy->SetUseGammaCorrection( bUseGammaCorrection );

	ElementBatcher = MakeShareable(new FSlateElementBatcher(RenderTargetPolicy.ToSharedRef()));
}

void FSlate3DRenderer::Cleanup()
{
	if ( RenderTargetPolicy.IsValid() )
	{
		RenderTargetPolicy->ReleaseResources();
	}

	BeginCleanup(this);
}

void FSlate3DRenderer::FinishCleanup()
{
	delete this;
}

FSlateDrawBuffer& FSlate3DRenderer::GetDrawBuffer()
{
	FreeBufferIndex = (FreeBufferIndex + 1) % NUM_DRAW_BUFFERS;
	FSlateDrawBuffer* Buffer = &DrawBuffers[FreeBufferIndex];

	while (!Buffer->Lock())
	{
		FlushRenderingCommands();

		UE_LOG(LogSlate, Log, TEXT("Slate: Had to block on waiting for a draw buffer"));
		FreeBufferIndex = (FreeBufferIndex + 1) % NumDrawBuffers;

		Buffer = &DrawBuffers[FreeBufferIndex];
	}

	Buffer->ClearBuffer();

	return *Buffer;
}

void FSlate3DRenderer::DrawWindow_GameThread(FSlateDrawBuffer& DrawBuffer)
{
	check( IsInGameThread() );

	const TSharedRef<FSlateFontCache> FontCache = SlateFontServices->GetGameThreadFontCache();

	TArray<TSharedPtr<FSlateWindowElementList>>& WindowElementLists = DrawBuffer.GetWindowElementLists();

	// Only one window element list for now.
	check( WindowElementLists.Num() == 1);

	FSlateWindowElementList& ElementList = *WindowElementLists[0];

	TSharedPtr<SWindow> Window = ElementList.GetWindow();

	if (Window.IsValid())
	{
		const FVector2D WindowSize = Window->GetSizeInScreen();
		if (WindowSize.X > 0 && WindowSize.Y > 0)
		{
			// Add all elements for this window to the element batcher
			ElementBatcher->AddElements(ElementList);

			// Update the font cache with new text after elements are batched
			FontCache->UpdateCache();

			// All elements for this window have been batched and rendering data updated
			ElementBatcher->ResetBatches();
		}
	}
}

template<typename TKeepAliveType>
struct TKeepAliveCommand : public FRHICommand < TKeepAliveCommand<TKeepAliveType> >
{
	TKeepAliveType Value;
	
	TKeepAliveCommand(TKeepAliveType InValue) : Value(InValue) {}

	void Execute(FRHICommandListBase& CmdList) {}
};

void FSlate3DRenderer::DrawWindowToTarget_RenderThread(FRHICommandListImmediate& InRHICmdList, FTextureRenderTarget2DResource* RenderTargetResource, FSlateDrawBuffer& WindowDrawBuffer)
{
	SCOPED_DRAW_EVENT(InRHICmdList, SlateRenderToTarget);

	checkSlow(WindowDrawBuffer.GetWindowElementLists().Num() == 1);
	checkSlow(RenderTargetResource);

	FSlateWindowElementList& WindowElementList = *WindowDrawBuffer.GetWindowElementLists()[0].Get();

	FSlateBatchData& BatchData = WindowElementList.GetBatchData();
	FElementBatchMap& RootBatchMap = WindowElementList.GetRootDrawLayer().GetElementBatchMap();

	WindowElementList.PreDraw_ParallelThread();

	BatchData.CreateRenderBatches(RootBatchMap);

	// Enqueue a command to unlock the draw buffer after all windows have been drawn
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(SlateBeginDrawingWindowsCommand,
		FSlateRHIRenderingPolicy&, Policy, *RenderTargetPolicy,
		{
			Policy.BeginDrawingWindows();
		});

	RenderTargetPolicy->UpdateVertexAndIndexBuffers( InRHICmdList, BatchData );
	
	// Set render target and clear.
	FTexture2DRHIRef RTResource = RenderTargetResource->GetTextureRHI();
	FRHIRenderTargetView ColorRTV(RTResource);
	ColorRTV.LoadAction = ERenderTargetLoadAction::EClear;
	FRHISetRenderTargetsInfo Info(1, &ColorRTV, FTextureRHIParamRef());
	Info.bClearColor = true;

	InRHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, RTResource);
	InRHICmdList.SetRenderTargetsAndClear(Info);

	FMatrix ProjectionMatrix = FSlateRHIRenderer::CreateProjectionMatrix(RTResource->GetSizeX(), RTResource->GetSizeY());
	if (BatchData.GetRenderBatches().Num() > 0)
	{
		FSlateBackBuffer BackBufferTarget(RenderTargetResource->GetTextureRHI(), FIntPoint(RTResource->GetSizeX(), RTResource->GetSizeY()));

		// The scene renderer will handle it in this case
		const bool bAllowSwitchVerticalAxis = false;

		RenderTargetPolicy->DrawElements
		(
			InRHICmdList,
			BackBufferTarget,
			ProjectionMatrix,
			BatchData.GetRenderBatches(),
			bAllowSwitchVerticalAxis
		);
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(SlateEndDrawingWindowsCommand,
		FSlateDrawBuffer*, DrawBuffer, &WindowDrawBuffer,
		FSlateRHIRenderingPolicy&, Policy, *RenderTargetPolicy,
		{
			FSlateEndDrawingWindowsCommand::EndDrawingWindows(RHICmdList, DrawBuffer, Policy);
		});

	InRHICmdList.CopyToResolveTarget(RenderTargetResource->GetTextureRHI(), RTResource, true, FResolveParams());

	ISlate3DRendererPtr Self = SharedThis(this);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(SlateDrawWindows_ResourceRetainer,
		ISlate3DRendererPtr, Renderer, Self,
		{
			new ( RHICmdList.AllocCommand<TKeepAliveCommand<ISlate3DRendererPtr>>() ) TKeepAliveCommand<ISlate3DRendererPtr>(Renderer);
		});
}
