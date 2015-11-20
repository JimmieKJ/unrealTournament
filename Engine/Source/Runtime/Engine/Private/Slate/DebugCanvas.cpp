// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SlateBasics.h"
#include "DebugCanvas.h"

/** Checks that all FCanvasProxy allocations were deleted */
class FProxyCounter
{
public:
	FProxyCounter()
	{
		Creations = 0;
		Deletions = 0;
	}

	~FProxyCounter()
	{
		ensureMsgf( Creations == Deletions, TEXT("FProxyCounter::~FProxyCounter has a mismatch.  %d creations != %d deletions"), Creations, Deletions );
	}

	int32 Creations;
	int32 Deletions;
};
	
class FCanvasProxy
{
public:
	FCanvasProxy( FRenderTarget* RenderTarget, UWorld* InWorld )
		: Canvas(RenderTarget, NULL, InWorld, InWorld ? InWorld->FeatureLevel : GMaxRHIFeatureLevel)
	{
		// Do not allow the canvas to be flushed outside of our debug rendering path
		Canvas.SetAllowedModes( 0 );
		++Counter.Creations;
	}

	~FCanvasProxy()
	{
		++Counter.Deletions;
	}

	/** The canvas on this proxy */
	FCanvas Canvas;
	static FProxyCounter Counter;
};

FProxyCounter FCanvasProxy::Counter;

/**
 * Simple representation of the backbuffer that the debug canvas renders to
 * This class may only be accessed from the render thread
 */
class FSlateCanvasRenderTarget : public FRenderTarget
{
public:
	/** FRenderTarget interface */
	virtual FIntPoint GetSizeXY() const
	{
		return ViewRect.Size();
	}

	/** Sets the texture that this target renders to */
	void SetRenderTargetTexture( FTexture2DRHIRef& InRHIRef )
	{
		RenderTargetTextureRHI = InRHIRef;
	}

	/** Clears the render target texture */
	void ClearRenderTargetTexture()
	{
		RenderTargetTextureRHI.SafeRelease();
	}

	/** Sets the viewport rect for the render target */
	void SetViewRect( const FIntRect& InViewRect ) 
	{ 
		ViewRect = InViewRect;
	}

	/** Gets the viewport rect for the render target */
	const FIntRect& GetViewRect() const 
	{
		return ViewRect; 
	}
private:
	FIntRect ViewRect;
};

FDebugCanvasDrawer::FDebugCanvasDrawer()
	: GameThreadCanvas( NULL )
	, RenderThreadCanvas( NULL )
	, RenderTarget( new FSlateCanvasRenderTarget )
{}

FDebugCanvasDrawer::~FDebugCanvasDrawer()
{
	delete RenderTarget;

	// We assume that the render thread is no longer utilizing any canvases
	if( GameThreadCanvas && RenderThreadCanvas != GameThreadCanvas )
	{
		delete GameThreadCanvas;
	}

	if( RenderThreadCanvas )
	{
		delete RenderThreadCanvas;
	}
}

FCanvas* FDebugCanvasDrawer::GetGameThreadDebugCanvas()
{
	return &GameThreadCanvas->Canvas;
}


void FDebugCanvasDrawer::BeginRenderingCanvas( const FIntRect& CanvasRect )
{
	if( CanvasRect.Size().X > 0 && CanvasRect.Size().Y > 0 )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER
		(
			BeginRenderingDebugCanvas,
			FDebugCanvasDrawer*, CanvasDrawer, this, 
			FCanvasProxy*, CanvasToRender, GameThreadCanvas,
			FIntRect, CanvasRect, CanvasRect,
			{
				// Delete the old rendering thread canvas
				if( CanvasDrawer->GetRenderThreadCanvas() && CanvasToRender != NULL )
				{
					CanvasDrawer->DeleteRenderThreadCanvas();
				}

				if( CanvasToRender == NULL )
				{
					CanvasToRender = CanvasDrawer->GetRenderThreadCanvas();
				}

				CanvasDrawer->SetRenderThreadCanvas( CanvasRect, CanvasToRender ); 
			}
		);
		
		// Gave the canvas to the render thread
		GameThreadCanvas = NULL;
	}
}


void FDebugCanvasDrawer::InitDebugCanvas(UWorld* InWorld)
{
	// If the canvas is not null there is more than one viewport draw call before slate draws.  This can happen on resizes. 
	// We need to delete the old canvas
		// This can also happen if we are debugging a HUD blueprint and in that case we need to continue using
		// the same canvas
	if (FSlateApplication::Get().IsNormalExecution())
	{
		if( GameThreadCanvas != NULL )
		{
			delete GameThreadCanvas;
		}

		GameThreadCanvas = new FCanvasProxy( RenderTarget, InWorld );
	}
}

void FDebugCanvasDrawer::DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer)
{
	check( IsInRenderingThread() );

	if( RenderThreadCanvas )
	{
		FTexture2DRHIRef& RT = *(FTexture2DRHIRef*)InWindowBackBuffer;
		RenderTarget->SetRenderTargetTexture( RT );
		bool bNeedToFlipVertical = RenderThreadCanvas->Canvas.GetAllowSwitchVerticalAxis();
		// Do not flip when rendering to the back buffer
		RenderThreadCanvas->Canvas.SetAllowSwitchVerticalAxis(false);
		if (RenderThreadCanvas->Canvas.IsScaledToRenderTarget() && IsValidRef(RT)) 
		{
			RenderThreadCanvas->Canvas.SetRenderTargetRect( FIntRect(0, 0, RT->GetSizeX(), RT->GetSizeY()) );
		}
		else
		{
			RenderThreadCanvas->Canvas.SetRenderTargetRect( RenderTarget->GetViewRect() );
		}
		RenderThreadCanvas->Canvas.Flush_RenderThread(RHICmdList, true);
		RenderThreadCanvas->Canvas.SetAllowSwitchVerticalAxis(bNeedToFlipVertical);
		RenderTarget->ClearRenderTargetTexture();
	}
}

FCanvasProxy* FDebugCanvasDrawer::GetRenderThreadCanvas() 
{
	check( IsInRenderingThread() );
	return RenderThreadCanvas;
}

void FDebugCanvasDrawer::DeleteRenderThreadCanvas()
{
	check( IsInRenderingThread() );
	if( RenderThreadCanvas )
	{
		delete RenderThreadCanvas;
		RenderThreadCanvas = NULL;
	}
}

void FDebugCanvasDrawer::SetRenderThreadCanvas( const FIntRect& InCanvasRect, FCanvasProxy* Canvas )
{
	check( IsInRenderingThread() );
	RenderTarget->SetViewRect( InCanvasRect );
	RenderThreadCanvas = Canvas;
}
