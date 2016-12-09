// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "GameLiveStreaming.h"
#include "UObject/Package.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "Engine/Texture2D.h"
#include "StaticBoundShaderState.h"
#include "RHIStaticStates.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Framework/Application/SlateApplication.h"
#include "Features/IModularFeatures.h"
#include "Features/ILiveStreamingService.h"
#include "ScreenRendering.h"

IMPLEMENT_MODULE( FGameLiveStreaming, GameLiveStreaming );

#define LOCTEXT_NAMESPACE "GameLiveStreaming"

FGameLiveStreaming::FGameLiveStreaming()
	: BroadcastStartCommand
		(
			TEXT( "GameLiveStreaming.StartBroadcasting" ),
			TEXT( "Starts broadcasting game video and audio using a live internet streaming service\n" )
			TEXT( "\n" )
			TEXT( "Optional parameters:\n" )
			TEXT( "     LoginUserName=<string>              For direct authentication, the user name to login as\n" )
			TEXT( "     LoginPassword=<string>              For direct authentication, the password to use\n" )
			TEXT( "     FrameRate=<int32>                   Frame rate to stream video when broadcasting\n")
			TEXT( "     ScreenScaling=<float>               How much to scale the broadcast video resolution down\n" )
			TEXT( "     bStartWebCam=<bool>				    If enabled, starts capturing video from your web camera right away\n" )
			TEXT( "     DesiredWebCamWidth=<int32>          Horizontal resolution for web camera capture\n" )
			TEXT( "     DesiredWebCamHeight=<int32>         Veritcal resolution for web camera capture\n" )
			TEXT( "     bMirrorWebCamImage=<bool>			Flips the web camera image horizontally\n" )
			TEXT( "     bDrawSimpleWebCamVideo=<bool>       Draws a simple web cam video on top of the viewport\n" )
			TEXT( "     bCaptureAudioFromComputer=<bool>    Enables capturing sound that is played back through your PC\n" )
			TEXT( "     bCaptureAudioFromMicrophone=<bool>  Enables capturing sound from your default microphone\n" )
			TEXT( "     CoverUpImage=<2D Texture Path>      Broadcasts the specified texture instead of what's on screen\n" ),
			FConsoleCommandWithArgsDelegate::CreateStatic(
				[]( const TArray< FString >& Args )
				{
					FGameBroadcastConfig Config;
					for( FString Arg : Args )
					{
						FParse::Value( *Arg, TEXT( "LoginUserName=" ), Config.LoginUserName );
						FParse::Value( *Arg, TEXT( "LoginPassword=" ), Config.LoginPassword );
						FParse::Value( *Arg, TEXT( "FrameRate="), Config.FrameRate );
						FParse::Value( *Arg, TEXT( "ScreenScaling="), Config.ScreenScaling );
						FParse::Bool( *Arg, TEXT( "bStartWebCam="), Config.bStartWebCam );
						FParse::Value( *Arg, TEXT( "DesiredWebCamWidth="), Config.WebCamConfig.DesiredWebCamWidth );
						FParse::Value( *Arg, TEXT( "DesiredWebCamHeight="), Config.WebCamConfig.DesiredWebCamHeight );
						FParse::Bool( *Arg, TEXT( "bMirrorWebCamImage="), Config.WebCamConfig.bMirrorWebCamImage );
						FParse::Bool( *Arg, TEXT( "bDrawSimpleWebCamVideo=" ), Config.WebCamConfig.bDrawSimpleWebCamVideo );
						FParse::Bool( *Arg, TEXT( "bCaptureAudioFromComputer="), Config.bCaptureAudioFromComputer );
						FParse::Bool( *Arg, TEXT( "bCaptureAudioFromMicrophone="), Config.bCaptureAudioFromMicrophone );
						
						FString CoverUpImagePath;
						FParse::Value( *Arg, TEXT( "CoverUpImage" ), CoverUpImagePath );
						if( !CoverUpImagePath.IsEmpty() )
						{
							Config.CoverUpImage = LoadObject<UTexture2D>( GetTransientPackage(), *CoverUpImagePath );
						}
					}
					IGameLiveStreaming::Get().StartBroadcastingGame( Config );
				} )
		),
	  BroadcastStopCommand
		(
			TEXT( "GameLiveStreaming.StopBroadcasting" ),
			TEXT( "Stops broadcasting game video" ),
			FConsoleCommandDelegate::CreateStatic( 
				[]
				{
					IGameLiveStreaming::Get().StopBroadcastingGame();
				} )
		),
	  WebCamStartCommand
		(
			TEXT( "GameLiveStreaming.StartWebCam" ),
			TEXT( "Starts capturing web camera video and previewing it on screen\n" )
			TEXT( "\n" )
			TEXT( "Optional parameters:\n" )
			TEXT( "     DesiredWebCamWidth=<int32>          Horizontal resolution for web camera capture\n" )
			TEXT( "     DesiredWebCamHeight=<int32>         Veritcal resolution for web camera capture\n" )
			TEXT( "     bMirrorWebCamImage=<bool>			Flips the web camera image horizontally\n" )
			TEXT( "     bDrawSimpleWebCamVideo=<bool>       Draws a simple web cam video on top of the viewport\n" ),
			FConsoleCommandWithArgsDelegate::CreateStatic(
				[]( const TArray< FString >& Args )
				{
					FGameWebCamConfig Config;
					for( FString Arg : Args )
					{
						FParse::Value( *Arg, TEXT( "DesiredWebCamWidth="), Config.DesiredWebCamWidth );
						FParse::Value( *Arg, TEXT( "DesiredWebCamHeight="), Config.DesiredWebCamHeight );
						FParse::Bool( *Arg, TEXT( "bMirrorWebCamImage="), Config.bMirrorWebCamImage );
						FParse::Bool( *Arg, TEXT( "bDrawSimpleWebCamVideo=" ), Config.bDrawSimpleWebCamVideo );
					}
					IGameLiveStreaming::Get().StartWebCam( Config );
				} )
		),
	  WebCamStopCommand
		(
			TEXT( "GameLiveStreaming.StopWebCam" ),
			TEXT( "Stops capturing web camera video" ),
			FConsoleCommandDelegate::CreateStatic( 
				[]
				{
					IGameLiveStreaming::Get().StopWebCam();
				} )
		)
{
	bIsBroadcasting = false;
	bIsWebCamEnabled = false;
	LiveStreamer = nullptr;
	ReadbackTextureIndex = 0;
	ReadbackBufferIndex = 0;
	ReadbackBuffers[0] = nullptr;
	ReadbackBuffers[1] = nullptr;
	bMirrorWebCamImage = false;
	bDrawSimpleWebCamVideo = true;
	CoverUpImage = nullptr;
}


void FGameLiveStreaming::StartupModule()
{
}


void FGameLiveStreaming::ShutdownModule()
{
	if( LiveStreamer != nullptr )
	{
		// Make sure the LiveStreaming plugin is actually still loaded.  During shutdown, it may have been unloaded before us,
		// and we can't dereference our LiveStreamer pointer if the memory was deleted already!
		static const FName LiveStreamingFeatureName( "LiveStreaming" );
		if( IModularFeatures::Get().IsModularFeatureAvailable( LiveStreamingFeatureName ) )	// Make sure the feature hasn't been destroyed before us
		{
			// Turn off the web camera.  It's important this happens first to avoid a shutdown crash.
			StopWebCam();

			// Stop broadcasting at shutdown
			StopBroadcastingGame();

			LiveStreamer->OnStatusChanged().RemoveAll( this );
			LiveStreamer->OnChatMessage().RemoveAll( this );
		}
		LiveStreamer = nullptr;
	}
}



bool FGameLiveStreaming::IsBroadcastingGame() const
{
	return bIsBroadcasting && LiveStreamer != nullptr && LiveStreamer->IsBroadcasting();
}


void FGameLiveStreaming::StartBroadcastingGame( const FGameBroadcastConfig& GameBroadcastConfig )
{
	if( !IsBroadcastingGame() )
	{
		if( bIsBroadcasting )
		{
			FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer().Get();
			SlateRenderer->OnSlateWindowRendered().RemoveAll( this );
			bIsBroadcasting = false;
		}

		// We can GetLiveStreamingService() here to fill in our LiveStreamer variable lazily, to make sure the service plugin is loaded 
		// before we try to cache it's interface pointer
		if( GetLiveStreamingService() != nullptr  )
		{
			CoverUpImage = GameBroadcastConfig.CoverUpImage;

			FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer().Get();
			SlateRenderer->OnSlateWindowRendered().AddRaw( this, &FGameLiveStreaming::OnSlateWindowRenderedDuringBroadcasting );

			FBroadcastConfig BroadcastConfig;
			BroadcastConfig.LoginUserName = GameBroadcastConfig.LoginUserName;
			BroadcastConfig.LoginPassword = GameBroadcastConfig.LoginPassword;

			// @todo livestream: This will interfere with editor live streaming if both are running at the same time!  The editor live 
			// streaming does check to make sure that game isn't already broadcasting, but the game currently doesn't have a good way to
			// do that, besides asking the LiveStreamer itself.

			UGameViewportClient* GameViewportClient = GEngine->GameViewport;
			check( GameViewportClient != nullptr );

			// @todo livestream: What about if viewport size changes while we are still broadcasting?  We need to restart the broadcast!
			BroadcastConfig.VideoBufferWidth = GameViewportClient->Viewport->GetSizeXY().X;
			BroadcastConfig.VideoBufferHeight = GameViewportClient->Viewport->GetSizeXY().Y;

			BroadcastConfig.VideoBufferWidth = FPlatformMath::FloorToInt( (float)BroadcastConfig.VideoBufferWidth * GameBroadcastConfig.ScreenScaling );
			BroadcastConfig.VideoBufferHeight = FPlatformMath::FloorToInt( (float)BroadcastConfig.VideoBufferHeight * GameBroadcastConfig.ScreenScaling );

			// Fix up the desired resolution so that it will work with the streaming system.  Some broadcasters require the
			// video buffer to be multiples of specific values, such as 32
			// @todo livestream: This could cause the aspect ratio to be changed and the buffer to be stretched non-uniformly, but usually the aspect only changes slightly
			LiveStreamer->MakeValidVideoBufferResolution( BroadcastConfig.VideoBufferWidth, BroadcastConfig.VideoBufferHeight );

			// Setup readback buffer textures
			{
				for( int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex )
				{
					ReadbackTextures[ TextureIndex ] = nullptr;
				}
				ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
					FWebMRecordCreateBufers,
					int32,InVideoWidth,BroadcastConfig.VideoBufferWidth,
					int32,InVideoHeight,BroadcastConfig.VideoBufferHeight,
					FTexture2DRHIRef*,InReadbackTextures,ReadbackTextures,
				{
					for (int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex)
					{
						FRHIResourceCreateInfo CreateInfo;
						InReadbackTextures[ TextureIndex ] = RHICreateTexture2D(
							InVideoWidth,
							InVideoHeight,
							PF_B8G8R8A8,
							1,
							1,
							TexCreate_CPUReadback,
							CreateInfo
							);
					}
				});
				FlushRenderingCommands();
				for (int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex)
				{
					check(ReadbackTextures[TextureIndex].GetReference());
				}
	
				ReadbackTextureIndex = 0;

				ReadbackBuffers[0] = nullptr;
				ReadbackBuffers[1] = nullptr;
				ReadbackBufferIndex = 0;
			}

			BroadcastConfig.FramesPerSecond = GameBroadcastConfig.FrameRate;
			BroadcastConfig.PixelFormat = FBroadcastConfig::EBroadcastPixelFormat::B8G8R8A8;	// Matches viewport backbuffer format
			BroadcastConfig.bCaptureAudioFromComputer = GameBroadcastConfig.bCaptureAudioFromComputer;
			BroadcastConfig.bCaptureAudioFromMicrophone = GameBroadcastConfig.bCaptureAudioFromMicrophone;
			LiveStreamer->StartBroadcasting( BroadcastConfig );

			if( GameBroadcastConfig.bStartWebCam && !bIsWebCamEnabled )
			{
				this->StartWebCam( GameBroadcastConfig.WebCamConfig );
			}

			bIsBroadcasting = true;
		}
	}
}


void FGameLiveStreaming::StopBroadcastingGame()
{
	if( bIsBroadcasting )
	{
		if( LiveStreamer != nullptr )
		{
			if( LiveStreamer->IsBroadcasting() )
			{
				LiveStreamer->StopBroadcasting();
			}
		}
		if( FSlateApplication::IsInitialized() )	// During shutdown, Slate may have already been destroyed by the time our viewport gets cleaned up
		{
			FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer().Get();
			SlateRenderer->OnSlateWindowRendered().RemoveAll( this );
		}

		// Cleanup readback buffer textures
		{
			FlushRenderingCommands();

			ReadbackTextures[0].SafeRelease();
			ReadbackTextures[1].SafeRelease();
			ReadbackTextureIndex = 0;
			ReadbackBuffers[0] = nullptr;
			ReadbackBuffers[1] = nullptr;
			ReadbackBufferIndex = 0;
		}
	}

	// Turn off the web camera, too.
	StopWebCam();
}


bool FGameLiveStreaming::IsWebCamEnabled() const
{
	return bIsWebCamEnabled && LiveStreamer != nullptr && LiveStreamer->IsWebCamEnabled();
}


void FGameLiveStreaming::StartWebCam( const FGameWebCamConfig& GameWebCamConfig )
{
	if( !bIsWebCamEnabled )
	{
		this->bMirrorWebCamImage = GameWebCamConfig.bMirrorWebCamImage;
		this->bDrawSimpleWebCamVideo = GameWebCamConfig.bDrawSimpleWebCamVideo;

		// We can GetLiveStreamingService() here to fill in our LiveStreamer variable lazily, to make sure the service plugin is loaded 
		// before we try to cache it's interface pointer
		if( GetLiveStreamingService() != nullptr )
		{
			FWebCamConfig WebCamConfig;
			WebCamConfig.DesiredWebCamWidth = GameWebCamConfig.DesiredWebCamWidth;
			WebCamConfig.DesiredWebCamHeight = GameWebCamConfig.DesiredWebCamHeight;

			LiveStreamer->StartWebCam( WebCamConfig );

			bIsWebCamEnabled = true;
		}
	}
}


void FGameLiveStreaming::StopWebCam()
{
	if( bIsWebCamEnabled )
	{
		if( LiveStreamer != nullptr )
		{
			if( LiveStreamer->IsWebCamEnabled() )
			{
				LiveStreamer->StopWebCam();
			}
		}

		bIsWebCamEnabled = false;
	}
}


void FGameLiveStreaming::OnSlateWindowRenderedDuringBroadcasting( SWindow& SlateWindow, void* ViewportRHIPtr )
{
	// If we're streaming live video/audio, we'll go ahead and push new video frames here
	UGameViewportClient* GameViewportClient = GEngine->GameViewport;
	if( IsBroadcastingGame() && GameViewportClient != nullptr )
	{
		// We only care about our own Slate window
		if( GameViewportClient->GetWindow() == SlateWindow.AsShared() )
		{
			// Check to see if we're streaming live video.  If so, we'll want to push new frames to be broadcast.
			if( LiveStreamer->IsReadyForVideoFrames() )
			{
				// Check to see if there are any video frames ready to push
				BroadcastGameVideoFrame();

				// Start copying next game video frame
				const FViewportRHIRef* ViewportRHI = ( const FViewportRHIRef* )ViewportRHIPtr;
				StartCopyingNextGameVideoFrame( *ViewportRHI );
			}
		}
	}
	else
	{
		// No longer broadcasting.  The live streaming service may have been interrupted.
		StopBroadcastingGame();
	}
}


void FGameLiveStreaming::BroadcastGameVideoFrame()
{
	if( ReadbackBuffers[ ReadbackBufferIndex ] != nullptr )
	{
		// Great, we have a new frame back from the GPU.  Upload the video frame!
		LiveStreamer->PushVideoFrame( (FColor*)ReadbackBuffers[ ReadbackBufferIndex ] );

		// Unmap the buffer now that we've pushed out the frame
		{
			struct FReadbackFromStagingBufferContext
			{
				FGameLiveStreaming* This;
			};
			FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
			{
				this
			};
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				ReadbackFromStagingBuffer,
				FReadbackFromStagingBufferContext,Context,ReadbackFromStagingBufferContext,
			{
				RHICmdList.UnmapStagingSurface(Context.This->ReadbackTextures[Context.This->ReadbackTextureIndex]);
			});
		}								
	}
	else
	{
		// Frame hasn't finished copying from the GPU, so we can't broadcast a frame yet
	}
}


void FGameLiveStreaming::StartCopyingNextGameVideoFrame( const FViewportRHIRef& ViewportRHI )
{
	// Check the video buffer size, in case we were configured to downscale the image
	FBroadcastConfig BroadcastConfig;
	LiveStreamer->QueryBroadcastConfig( BroadcastConfig );

	const FIntPoint ResizeTo( BroadcastConfig.VideoBufferWidth, BroadcastConfig.VideoBufferHeight );

	static const FName RendererModuleName( "Renderer" );
	IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>( RendererModuleName );

	UGameViewportClient* GameViewportClient = GEngine->GameViewport;
	check( GameViewportClient != nullptr );

	struct FCopyVideoFrame
	{
		FViewportRHIRef ViewportRHI;
		IRendererModule* RendererModule;
		FIntPoint ResizeTo;
		FTexture2DRHIRef CoverUpImage;
		FGameLiveStreaming* This;
	};
	FCopyVideoFrame CopyVideoFrame =
	{
		ViewportRHI,
		&RendererModule,
		ResizeTo,
		CoverUpImage != nullptr ? ( (FTexture2DResource*)( CoverUpImage->Resource ) )->GetTexture2DRHI() : nullptr,
		this
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadSurfaceCommand,
		FCopyVideoFrame,Context,CopyVideoFrame,
	{
		FPooledRenderTargetDesc OutputDesc(FPooledRenderTargetDesc::Create2DDesc(Context.ResizeTo, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
			
		const auto FeatureLevel = GMaxRHIFeatureLevel;

		TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
		Context.RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("ResampleTexture"));
		check( ResampleTexturePooledRenderTarget );

		const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		RHICmdList.SetViewport(0, 0, 0.0f, Context.ResizeTo.X, Context.ResizeTo.Y, 1.0f);

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());

		FTexture2DRHIRef SourceTexture = 
			Context.CoverUpImage.IsValid() ? Context.CoverUpImage : RHICmdList.GetViewportBackBuffer(Context.ViewportRHI);

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, Context.RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

		if( Context.ResizeTo != FIntPoint( SourceTexture->GetSizeX(), SourceTexture->GetSizeY() ) )
		{
			// We're scaling down the window, so use bilinear filtering
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SourceTexture);
		}
		else
		{
			// Drawing 1:1, so no filtering needed
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTexture);
		}

		Context.RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,		// Dest X, Y
			Context.ResizeTo.X, Context.ResizeTo.Y,	// Dest Width, Height
			0, 0,		// Source U, V
			1, 1,		// Source USize, VSize
			Context.ResizeTo,		// Target buffer size
			FIntPoint(1, 1),		// Source texture size
			*VertexShader,
			EDRF_Default);

		// Asynchronously copy render target from GPU to CPU
		const bool bKeepOriginalSurface = false;
		RHICmdList.CopyToResolveTarget(
			DestRenderTarget.TargetableTexture,
			Context.This->ReadbackTextures[ Context.This->ReadbackTextureIndex ],
			bKeepOriginalSurface,
			FResolveParams());
	});
							

	// Start mapping the newly-rendered buffer
	{
		struct FReadbackFromStagingBufferContext
		{
			FGameLiveStreaming* This;
		};
		FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
		{
			this
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReadbackFromStagingBuffer,
			FReadbackFromStagingBufferContext,Context,ReadbackFromStagingBufferContext,
		{
			int32 UnusedWidth = 0;
			int32 UnusedHeight = 0;
			RHICmdList.MapStagingSurface(Context.This->ReadbackTextures[Context.This->ReadbackTextureIndex], Context.This->ReadbackBuffers[Context.This->ReadbackBufferIndex], UnusedWidth, UnusedHeight);

			// Ping pong between readback textures
			Context.This->ReadbackTextureIndex = ( Context.This->ReadbackTextureIndex + 1 ) % 2;
		});
	}

	// Ping pong between readback buffers
	ReadbackBufferIndex = ( ReadbackBufferIndex + 1 ) % 2;
}


void FGameLiveStreaming::DrawSimpleWebCamVideo( UCanvas* Canvas )
{
	if( IsWebCamEnabled() && bDrawSimpleWebCamVideo && LiveStreamer->IsWebCamEnabled() )
	{
		bool bIsImageFlippedHorizontally = false;
		bool bIsImageFlippedVertically = false;
		UTexture2D* WebCamTexture = LiveStreamer->GetWebCamTexture( bIsImageFlippedHorizontally, bIsImageFlippedVertically );
		if( WebCamTexture != nullptr )
		{
			// Give the user a chance to customize the image mirroring
			if( this->bMirrorWebCamImage )
			{
				bIsImageFlippedHorizontally = !bIsImageFlippedHorizontally;
			}

			const float BorderPadding = 6.0f;
			Canvas->Canvas->DrawTile( 
				Canvas->SizeX - WebCamTexture->GetSizeX() - BorderPadding, BorderPadding,	// Top right justify
				WebCamTexture->GetSizeX(), WebCamTexture->GetSizeY(),
				bIsImageFlippedHorizontally ? 1.0f : 0.0f, bIsImageFlippedVertically ? 1.0f : 0.0f, 
				bIsImageFlippedHorizontally ? -1.0f : 1.0f, bIsImageFlippedVertically ? -1.0f : 1.0f, 
				FLinearColor::White, 
				WebCamTexture->Resource, 
				false );	// Alpha blend?
		}
	}
}


UTexture2D* FGameLiveStreaming::GetWebCamTexture( bool& bIsImageFlippedHorizontally, bool& bIsImageFlippedVertically )
{
	bIsImageFlippedHorizontally = false;
	bIsImageFlippedVertically = false;

	UTexture2D* WebCamTexture = nullptr;
	if( IsWebCamEnabled() && bDrawSimpleWebCamVideo && LiveStreamer->IsWebCamEnabled() )
	{
		WebCamTexture = LiveStreamer->GetWebCamTexture( bIsImageFlippedHorizontally, bIsImageFlippedVertically );
	}
	return WebCamTexture;
}


ILiveStreamingService* FGameLiveStreaming::GetLiveStreamingService()
{
	if( LiveStreamer == nullptr )
	{
		static const FName LiveStreamingFeatureName( "LiveStreaming" );
		if( IModularFeatures::Get().IsModularFeatureAvailable( LiveStreamingFeatureName ) )
		{
			// Select a live streaming service
			LiveStreamer = &IModularFeatures::Get().GetModularFeature<ILiveStreamingService>( LiveStreamingFeatureName );

			// Register to find out about status changes
			LiveStreamer->OnStatusChanged().AddRaw( this, &FGameLiveStreaming::BroadcastStatusCallback );
			LiveStreamer->OnChatMessage().AddRaw( this, &FGameLiveStreaming::OnChatMessage );
		}
	}
	return LiveStreamer;
}


	
void FGameLiveStreaming::BroadcastStatusCallback( const FLiveStreamingStatus& Status )
{
	// @todo livestream: Hook this up to provide C++ users and Blueprints with status about the live streaming startup/webcam/shutdown
}


void FGameLiveStreaming::OnChatMessage( const FText& UserName, const FText& Text )
{
	// @todo livestream: Add support (and also, connecting, disconnecting, sending messages, etc.)
}


void FGameLiveStreaming::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( CoverUpImage );
}


#undef LOCTEXT_NAMESPACE
