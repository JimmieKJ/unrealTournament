// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRendererPrivate.h"

#include "OpenGL/SlateOpenGLRenderer.h"
#include "OpenGL/SlateOpenGLTextureManager.h"
#include "OpenGL/SlateOpenGLRenderingPolicy.h"
#include "FontCache.h"
#include "ElementBatcher.h"

FSlateOpenGLRenderer::FSlateOpenGLRenderer( const ISlateStyle& InStyle )
:	Style( InStyle )
{

	ViewMatrix = FMatrix(	FPlane(1,	0,	0,	0),
							FPlane(0,	1,	0,	0),
							FPlane(0,	0,	1,  0),
							FPlane(0,	0,	0,	1));

}

FSlateOpenGLRenderer::~FSlateOpenGLRenderer()
{
}

/** Returns a draw buffer that can be used by Slate windows to draw window elements */
FSlateDrawBuffer& FSlateOpenGLRenderer::GetDrawBuffer()
{
	// Clear out the buffer each time its accessed
	DrawBuffer.ClearBuffer();
	return DrawBuffer;
}

class FSlateOpenGLFontAtlasFactory : public ISlateFontAtlasFactory
{
public:
	virtual ~FSlateOpenGLFontAtlasFactory()
	{
	}

	virtual FIntPoint GetAtlasSize() const override
	{
		return FIntPoint(TextureSize, TextureSize);
	}

	virtual TSharedRef<FSlateFontAtlas> CreateFontAtlas() const override
	{
		TSharedRef<FSlateFontTextureOpenGL> FontTexture = MakeShareable( new FSlateFontTextureOpenGL( TextureSize, TextureSize ) );
		FontTexture->CreateFontTexture();

		return FontTexture;
	}

private:

	/** Size of each font texture, width and height */
	static const uint32 TextureSize = 1024;
};

void FSlateOpenGLRenderer::Initialize()
{
	SharedContext.Initialize( NULL, NULL );

	/** Size of each font texture, width and height */
	const uint32 TextureSize = 1024;

	TextureManager = MakeShareable( new FSlateOpenGLTextureManager );

	FontCache = MakeShareable( new FSlateFontCache( MakeShareable( new FSlateOpenGLFontAtlasFactory ) ) );
	FontMeasure = FSlateFontMeasure::Create( FontCache.ToSharedRef() );

	RenderingPolicy = MakeShareable( new FSlateOpenGLRenderingPolicy( FontCache, TextureManager ) );

	ElementBatcher = MakeShareable( new FSlateElementBatcher( RenderingPolicy.ToSharedRef() ) );

#if !PLATFORM_USES_ES2
	// Load OpenGL extensions if needed.  Need a current rendering context to do this
	LoadOpenGLExtensions();
#endif

	TextureManager->LoadUsedTextures();

	// Create rendering resources if needed
	RenderingPolicy->ConditionalInitializeResources();
}

/** 
 * Creates necessary resources to render a window and sends draw commands to the rendering thread
 *
 * @param WindowDrawBuffer	The buffer containing elements to draw 
 */
void FSlateOpenGLRenderer::DrawWindows( FSlateDrawBuffer& InWindowDrawBuffer )
{
	// Update the font cache with new text before elements are batched
	FontCache->UpdateCache();

	// Draw each window.  For performance.  All elements are batched before anything is rendered
	TArray<FSlateWindowElementList>& WindowElementLists = InWindowDrawBuffer.GetWindowElementLists();

	for( int32 ListIndex = 0; ListIndex < WindowElementLists.Num(); ++ListIndex )
	{
		FSlateWindowElementList& ElementList = WindowElementLists[ListIndex];

		if ( ElementList.GetWindow().IsValid() )
		{
			TSharedRef<SWindow> WindowToDraw = ElementList.GetWindow().ToSharedRef();

			const FVector2D WindowSize = WindowToDraw->GetSizeInScreen();
			FSlateOpenGLViewport* Viewport = WindowToViewportMap.Find( &WindowToDraw.Get() );
			check(Viewport);
		
			//@todo Slate OpenGL: Move this to ResizeViewport
			if( WindowSize.X != Viewport->ViewportRect.Right || WindowSize.Y != Viewport->ViewportRect.Bottom )
			{
				//@todo implement fullscreen
				const bool bFullscreen = false;
				Private_ResizeViewport( WindowSize, *Viewport, bFullscreen );
			}
			
			Viewport->MakeCurrent();

			// Batch elements.  Note that we must set the current viewport before doing this so we have a valid rendering context when calling OpenGL functions
			ElementBatcher->AddElements( ElementList.GetDrawElements() );

			//@ todo Slate: implement for opengl
			bool bRequiresStencilTest = false;
			ElementBatcher->FillBatchBuffers( ElementList, bRequiresStencilTest );

			ElementBatcher->ResetBatches();

			RenderingPolicy->UpdateBuffers( ElementList );

			check(Viewport);

			glViewport( Viewport->ViewportRect.Left, Viewport->ViewportRect.Top, Viewport->ViewportRect.Right, Viewport->ViewportRect.Bottom );

			// Draw all elements
			RenderingPolicy->DrawElements( ViewMatrix*Viewport->ProjectionMatrix, ElementList.GetRenderBatches() );

			Viewport->SwapBuffers();

			// Reset all batch data for this window
			ElementBatcher->ResetBatches();
		}
	}

	FontCache->ConditionalFlushCache();
}


/** Called when a window is destroyed to give the renderer a chance to free resources */
void FSlateOpenGLRenderer::OnWindowDestroyed( const TSharedRef<SWindow>& InWindow )
{
	FSlateOpenGLViewport* Viewport = WindowToViewportMap.Find( &InWindow.Get() );
	if( Viewport )
	{
		Viewport->Destroy();
	}
	WindowToViewportMap.Remove( &InWindow.Get() );

	SharedContext.MakeCurrent();
}

void FSlateOpenGLRenderer::CreateViewport( const TSharedRef<SWindow> InWindow )
{
#if UE_BUILD_DEBUG
	// Ensure a viewport for this window doesnt already exist
	FSlateOpenGLViewport* Viewport = WindowToViewportMap.Find( &InWindow.Get() );
	check(!Viewport);
#endif

	FSlateOpenGLViewport& NewViewport = WindowToViewportMap.Add( &InWindow.Get(), FSlateOpenGLViewport() );
	NewViewport.Initialize( InWindow, SharedContext );
}


void FSlateOpenGLRenderer::RequestResize( const TSharedPtr<SWindow>& InWindow, uint32 NewSizeX, uint32 NewSizeY )
{
	// @todo implement.  Viewports are currently resized in DrawWindows
}

void FSlateOpenGLRenderer::Private_ResizeViewport( const FVector2D& WindowSize, FSlateOpenGLViewport& InViewport, bool bFullscreen )
{
	uint32 Width = FMath::TruncToInt(WindowSize.X);
	uint32 Height = FMath::TruncToInt(WindowSize.Y);

	InViewport.Resize( Width, Height, bFullscreen );
}

void FSlateOpenGLRenderer::UpdateFullscreenState( const TSharedRef<SWindow> InWindow, uint32 OverrideResX, uint32 OverrideResY )
{
	FSlateOpenGLViewport* Viewport = WindowToViewportMap.Find( &InWindow.Get() );
	 
	if( Viewport )
	{
		bool bFullscreen = IsViewportFullscreen( *InWindow );

		// todo: support Fullscreen modes in OpenGL
//		uint32 ResX = OverrideResX ? OverrideResX : GSystemResolution.ResX;
//		uint32 ResY = OverrideResY ? OverrideResY : GSystemResolution.ResY;

		Private_ResizeViewport( FVector2D( Viewport->ViewportRect.Right, Viewport->ViewportRect.Bottom ), *Viewport, bFullscreen );
	}
}


void FSlateOpenGLRenderer::ReleaseDynamicResource( const FSlateBrush& Brush )
{
	ensure( IsInGameThread() );
	TextureManager->ReleaseDynamicTextureResource( Brush );
}


bool FSlateOpenGLRenderer::GenerateDynamicImageResource(FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes)
{
	ensure( IsInGameThread() );
	return TextureManager->CreateDynamicTextureResource(ResourceName, Width, Height, Bytes) != NULL;
}


void FSlateOpenGLRenderer::LoadStyleResources( const ISlateStyle& InStyle )
{
	if ( TextureManager.IsValid() )
	{
		TextureManager->LoadStyleResources( InStyle );
	}
}

FSlateUpdatableTexture* FSlateOpenGLRenderer::CreateUpdatableTexture(uint32 Width, uint32 Height)
{
	TArray<uint8> RawData;
	RawData.AddZeroed(Width * Height * 4);
	FSlateOpenGLTexture* NewTexture = new FSlateOpenGLTexture(Width, Height);
#if !PLATFORM_USES_ES2
	NewTexture->Init(GL_SRGB8_ALPHA8, RawData);
#else
	NewTexture->Init(GL_SRGB8_ALPHA8_EXT, RawData);
#endif
	return NewTexture;
}

void FSlateOpenGLRenderer::ReleaseUpdatableTexture(FSlateUpdatableTexture* Texture)
{
	delete Texture;
}

ISlateAtlasProvider* FSlateOpenGLRenderer::GetTextureAtlasProvider()
{
	if( TextureManager.IsValid() )
	{
		return TextureManager->GetTextureAtlasProvider();
	}

	return nullptr;
}
