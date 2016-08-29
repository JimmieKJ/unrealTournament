// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "EditorLiveStreamingModule.h"
#include "EditorLiveStreaming.h"
#include "EditorLiveStreamingSettings.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Runtime/Engine/Public/Features/ILiveStreamingService.h"
#include "Runtime/SlateCore/Public/Rendering/SlateRenderer.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "Runtime/GameLiveStreaming/Public/IGameLiveStreaming.h"
#include "ISettingsModule.h"
#include "SNotificationList.h"
#include "ModuleManager.h"
#include "NotificationManager.h"


IMPLEMENT_MODULE( FEditorLiveStreaming, EditorLiveStreaming );

#define LOCTEXT_NAMESPACE "EditorLiveStreaming"


FEditorLiveStreaming::FEditorLiveStreaming()
	: bIsBroadcasting( false ),
	  LiveStreamer( nullptr ),
	  SubmittedVideoFrameCount( 0 ),
	  ReadbackBufferIndex( 0 )
{
	ReadbackBuffers[0].Reset();
	ReadbackBuffers[1].Reset();
}


void FEditorLiveStreaming::StartupModule()
{
	// Register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if( ensure( SettingsModule != nullptr ) )
	{
		SettingsModule->RegisterSettings( "Editor", "Advanced", "EditorLiveStreaming",
			LOCTEXT( "EditorLiveStreamingSettingsName", "Live Streaming"),
			LOCTEXT( "EditorLiveStreamingSettingsDescription", "Settings for broadcasting live from the editor"),
			GetMutableDefault< UEditorLiveStreamingSettings >()
		);
	}
}


void FEditorLiveStreaming::ShutdownModule()
{
	// Unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if( SettingsModule != nullptr )
	{
		SettingsModule->UnregisterSettings( "Editor", "General", "EditorLiveStreaming" );
	}
}


bool FEditorLiveStreaming::IsLiveStreamingAvailable() const
{
	bool bIsAvailable = false;

	// This feature is currently "experimental" and disabled by default, even if you have a valid live streaming plugin installed
	if( GetDefault<UEditorExperimentalSettings>()->bLiveStreamingFromEditor )
	{
		// We are not available if the game is already streaming
		if( !IGameLiveStreaming::Get().IsBroadcastingGame() )
		{
			static const FName LiveStreamingFeatureName( "LiveStreaming" );
			if( IModularFeatures::Get().IsModularFeatureAvailable( LiveStreamingFeatureName ) )
			{
				// Only if not already broadcasting
				bIsAvailable = true;
			}
		}
	}

	return bIsAvailable;
}


bool FEditorLiveStreaming::IsBroadcastingEditor() const
{
	return bIsBroadcasting;
}


void FEditorLiveStreaming::StartBroadcastingEditor()
{
	if( !IsBroadcastingEditor() && IsLiveStreamingAvailable() )
	{
		// Select a live streaming service
		{
			static const FName LiveStreamingFeatureName( "LiveStreaming" );
			LiveStreamer = &IModularFeatures::Get().GetModularFeature<ILiveStreamingService>( LiveStreamingFeatureName );
		}

		// Register to find out about status changes
		LiveStreamer->OnStatusChanged().AddRaw( this, &FEditorLiveStreaming::BroadcastStatusCallback );

		// @todo livestream: Allow connection to chat independently from broadcasting? (see removing delegate too)
		LiveStreamer->OnChatMessage().AddRaw( this, &FEditorLiveStreaming::OnChatMessage );


		// Tell our live streaming plugin to start broadcasting
		{
			const auto& Settings = *GetDefault< UEditorLiveStreamingSettings >();
			FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer().Get();
			const FIntRect VirtualScreen = SlateRenderer->SetupVirtualScreenBuffer( Settings.bPrimaryMonitorOnly, Settings.ScreenScaling, LiveStreamer );

			bIsBroadcasting = true;
			SubmittedVideoFrameCount = 0;

			// @todo livestream: What about if virtual screen size changes while we are still broadcasting?  For example, if the user changes their
			// desktop resolution while the editor is running.  We'd need to stop and restart the broadcast.
			FBroadcastConfig BroadcastConfig;
			BroadcastConfig.VideoBufferWidth = VirtualScreen.Width();
			BroadcastConfig.VideoBufferHeight = VirtualScreen.Height();
			BroadcastConfig.FramesPerSecond = Settings.FrameRate;
			BroadcastConfig.PixelFormat = FBroadcastConfig::EBroadcastPixelFormat::R8G8B8A8;
			BroadcastConfig.bCaptureAudioFromComputer = Settings.bCaptureAudioFromComputer;
			BroadcastConfig.bCaptureAudioFromMicrophone = Settings.bCaptureAudioFromMicrophone;
			LiveStreamer->StartBroadcasting( BroadcastConfig );

			if( Settings.bEnableWebCam )
			{
				FWebCamConfig WebCamConfig;
				switch( Settings.WebCamResolution )
				{
					case EEditorLiveStreamingWebCamResolution::Normal_320x240:
						WebCamConfig.DesiredWebCamWidth = 320;
						WebCamConfig.DesiredWebCamHeight = 240;
						break;
					case EEditorLiveStreamingWebCamResolution::Wide_320x180:
						WebCamConfig.DesiredWebCamWidth = 320;
						WebCamConfig.DesiredWebCamHeight = 180;
						break;
					case EEditorLiveStreamingWebCamResolution::Normal_640x480:
						WebCamConfig.DesiredWebCamWidth = 640;
						WebCamConfig.DesiredWebCamHeight = 480;
						break;
					case EEditorLiveStreamingWebCamResolution::Wide_640x360:
						WebCamConfig.DesiredWebCamWidth = 640;
						WebCamConfig.DesiredWebCamHeight = 360;
						break;
					case EEditorLiveStreamingWebCamResolution::Normal_800x600:
						WebCamConfig.DesiredWebCamWidth = 800;
						WebCamConfig.DesiredWebCamHeight = 600;
						break;
					case EEditorLiveStreamingWebCamResolution::Wide_800x450:
						WebCamConfig.DesiredWebCamWidth = 800;
						WebCamConfig.DesiredWebCamHeight = 450;
						break;
					case EEditorLiveStreamingWebCamResolution::Normal_1024x768:
						WebCamConfig.DesiredWebCamWidth = 1024;
						WebCamConfig.DesiredWebCamHeight = 768;
						break;
					case EEditorLiveStreamingWebCamResolution::Wide_1024x576:
						WebCamConfig.DesiredWebCamWidth = 1024;
						WebCamConfig.DesiredWebCamHeight = 576;
						break;
					case EEditorLiveStreamingWebCamResolution::Normal_1080x810:
						WebCamConfig.DesiredWebCamWidth = 1080;
						WebCamConfig.DesiredWebCamHeight = 810;
						break;
					case EEditorLiveStreamingWebCamResolution::Wide_1080x720:
						WebCamConfig.DesiredWebCamWidth = 1080;
						WebCamConfig.DesiredWebCamHeight = 720;
						break;
					case EEditorLiveStreamingWebCamResolution::Normal_1280x960:
						WebCamConfig.DesiredWebCamWidth = 1280;
						WebCamConfig.DesiredWebCamHeight = 960;
						break;
					case EEditorLiveStreamingWebCamResolution::Wide_1280x720:
						WebCamConfig.DesiredWebCamWidth = 1280;
						WebCamConfig.DesiredWebCamHeight = 720;
						break;
					case EEditorLiveStreamingWebCamResolution::Normal_1920x1440:
						WebCamConfig.DesiredWebCamWidth = 1920;
						WebCamConfig.DesiredWebCamHeight = 1440;
						break;
					case EEditorLiveStreamingWebCamResolution::Wide_1920x1080:
						WebCamConfig.DesiredWebCamWidth = 1920;
						WebCamConfig.DesiredWebCamHeight = 1080;
						break;

					default:
						check(0);
						break;
				}

				// @todo livestream: Allow web cam to be started/stopped independently from the broadcast itself, so users can setup their web cam
				LiveStreamer->StartWebCam( WebCamConfig );
			}
		}
	}
}


void FEditorLiveStreaming::StopBroadcastingEditor()
{
	if( IsBroadcastingEditor() )
	{
		CloseBroadcastStatusWindow();

		LiveStreamer->StopWebCam();
		LiveStreamer->StopBroadcasting();

		// Unregister for status changes
		LiveStreamer->OnStatusChanged().RemoveAll( this );
		LiveStreamer->OnChatMessage().RemoveAll( this );

		LiveStreamer = nullptr;

		// Tell our notification to start expiring
		TSharedPtr< SNotificationItem > Notification( NotificationWeakPtr.Pin() );
		if( Notification.IsValid() )
		{
			Notification->ExpireAndFadeout();
		}

		bIsBroadcasting = false;
		for( int32 BufferIndex = 0; BufferIndex < 2; ++BufferIndex )
		{
			ReadbackBuffers[BufferIndex].Reset();
		}
		ReadbackBufferIndex = 0;
		SubmittedVideoFrameCount = 0;
	}
}


void FEditorLiveStreaming::BroadcastEditorVideoFrame()
{
	if( ensure( IsBroadcastingEditor() ) )
	{
		// Check to see if we're streaming live video.  If so, we'll want to push new frames to be broadcast.
		if( LiveStreamer->IsBroadcasting() )
		{
			// Is this live streamer ready to accept new frames?
			if( LiveStreamer->IsReadyForVideoFrames() )
			{
				FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer().Get();

				const FMappedTextureBuffer& CurrentBuffer = ReadbackBuffers[ReadbackBufferIndex];

				if ( CurrentBuffer.IsValid() )
				{
					//TODO PushVideoFrame Needs to Take Width and Height
					LiveStreamer->PushVideoFrame((FColor*)CurrentBuffer.Data/*, CurrentBuffer.Width, CurrentBuffer.Height*/);
					++SubmittedVideoFrameCount;

					// If this is the first frame we've submitted, then we can fade out our notification UI, since broadcasting is in progress
					if( SubmittedVideoFrameCount == 1 )
					{
						TSharedPtr< SNotificationItem > Notification( NotificationWeakPtr.Pin() );
						if( Notification.IsValid() )
						{
							Notification->ExpireAndFadeout();
						}
					}

					SlateRenderer->UnmapVirtualScreenBuffer();
				}

				TArray<FString> UnusedKeypressBuffer;
				SlateRenderer->CopyWindowsToVirtualScreenBuffer( UnusedKeypressBuffer );
				SlateRenderer->MapVirtualScreenBuffer(&ReadbackBuffers[ReadbackBufferIndex]);

				// Ping pong between buffers
				ReadbackBufferIndex = ( ReadbackBufferIndex + 1 ) % 2;
			}
		}
		else
		{
			// If our streaming service has stopped broadcasting for some reason, then we'll cancel our broadcast
			StopBroadcastingEditor();
		}
	}
}


void FEditorLiveStreaming::BroadcastStatusCallback( const FLiveStreamingStatus& Status )
{
	TSharedPtr< SNotificationItem > Notification( NotificationWeakPtr.Pin() );
	if( Notification.IsValid() )
	{
		// Don't bother clobbering existing message with text about web cam starting/stopping, unless we're already broadcasting.
		// We want to make sure people see the persistent text about login state.
		if( SubmittedVideoFrameCount > 0 ||
			( Status.StatusType != FLiveStreamingStatus::EStatusType::WebCamStarted &&
			  Status.StatusType != FLiveStreamingStatus::EStatusType::WebCamStopped &&
			  Status.StatusType != FLiveStreamingStatus::EStatusType::WebCamTextureNotReady &&
			  Status.StatusType != FLiveStreamingStatus::EStatusType::WebCamTextureReady &&
			  Status.StatusType != FLiveStreamingStatus::EStatusType::ChatConnected &&
			  Status.StatusType != FLiveStreamingStatus::EStatusType::ChatDisconnected ) )
		{
			Notification->SetText( Status.CustomStatusDescription );
		}
	}
	else
	{
		// Only spawn a notification if we're actually still trying to broadcast, not if we're stopping broadcasting.  We don't want
		// to revive our notification that we intentionally expired.
		if( bIsBroadcasting )
		{
			FNotificationInfo Info( Status.CustomStatusDescription );
			Info.FadeInDuration = 0.1f;
			Info.FadeOutDuration = 0.5f;
			Info.ExpireDuration = 1.5f;
			Info.bUseThrobber = false;
			Info.bUseSuccessFailIcons = true;
			Info.bUseLargeFont = true;
			Info.bFireAndForget = false;
			Info.bAllowThrottleWhenFrameRateIsLow = false;
			NotificationWeakPtr = FSlateNotificationManager::Get().AddNotification( Info );
		}
	}

	Notification = NotificationWeakPtr.Pin();
	if( Notification.IsValid() )
	{
		SNotificationItem::ECompletionState State = SNotificationItem::CS_Pending;
		if( Status.StatusType == FLiveStreamingStatus::EStatusType::Failure )
		{
			State = SNotificationItem::CS_Fail;
		}
		else if( Status.StatusType == FLiveStreamingStatus::EStatusType::BroadcastStarted ||
				 Status.StatusType == FLiveStreamingStatus::EStatusType::WebCamStarted ||
				 Status.StatusType == FLiveStreamingStatus::EStatusType::ChatConnected )
		{
			State = SNotificationItem::CS_Success;
		}

		Notification->SetCompletionState( State );
	}

	// If the web cam just turned on, then we'll go ahead and show it
	if( Status.StatusType == FLiveStreamingStatus::EStatusType::WebCamTextureReady )
	{	
		bool bIsImageFlippedHorizontally = false;
		bool bIsImageFlippedVertically = false;
		UTexture2D* WebCamTexture = LiveStreamer->GetWebCamTexture( bIsImageFlippedHorizontally, bIsImageFlippedVertically );
		if( ensure( WebCamTexture != nullptr ) )
		{
			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( TEXT( "MainFrame" ) );
			check( MainFrameModule.IsWindowInitialized() );
			auto RootWindow = MainFrameModule.GetParentWindow();
			check( RootWindow.IsValid() );

			// Allow the user to customize the image mirroring, too!
			const auto& Settings = *GetDefault< UEditorLiveStreamingSettings >();
			if( Settings.bMirrorWebCamImage )
			{
				bIsImageFlippedHorizontally = !bIsImageFlippedHorizontally;
			}

			// How many pixels from the edge of the main frame window to where the broadcast status window appears
			const int WindowBorderPadding = 50;

			// @todo livestream: Currently this window is not created as "topmost".  We don't really want it to be OS-topmost, but we do want it to be the most
			// topmost "regular" window in our application (not on top of tooltips, etc.)

			// Create a window that will show the web cam video feed
			BroadcastStatusWindow = SNew( SWindow )
				.Title( LOCTEXT( "StreamingWindowTitle", "Web Camera" ) )
				.ClientSize( FVector2D( WebCamTexture->GetSizeX(), WebCamTexture->GetSizeY() ) )
				.ScreenPosition( FVector2D( 
					RootWindow->GetRectInScreen().Right - WebCamTexture->GetSizeX() - WindowBorderPadding, 
					RootWindow->GetRectInScreen().Top +  WindowBorderPadding ) )

				// @todo livestream: Ideally the user could freely resize the window, but it gets a bit tricky to preserve the web cam aspect.  Plus, we don't really like how letterboxing/columnboxing looks with this.
				.SizingRule( ESizingRule::FixedSize )

				.AutoCenter( EAutoCenter::None )
				.bDragAnywhere( true )
				.SupportsMaximize( true )
				.SupportsMinimize( true )
				.FocusWhenFirstShown( false )
				.ActivateWhenFirstShown( false )
				.SaneWindowPlacement( false );

			WebCamDynamicImageBrush = MakeShareable( new FSlateDynamicImageBrush( 
				WebCamTexture,
				FVector2D( WebCamTexture->GetSizeX(), WebCamTexture->GetSizeY() ),
				WebCamTexture->GetFName() ) );

			// If the web cam image is coming in flipped, we'll apply mirroring to the Slate brush
			if( bIsImageFlippedHorizontally && bIsImageFlippedVertically )
			{
				WebCamDynamicImageBrush->Mirroring = ESlateBrushMirrorType::Both;
			}
			else if( bIsImageFlippedHorizontally )
			{ 
				WebCamDynamicImageBrush->Mirroring = ESlateBrushMirrorType::Horizontal;
			}
			else if( bIsImageFlippedVertically )
			{ 
				WebCamDynamicImageBrush->Mirroring = ESlateBrushMirrorType::Vertical;
			}

			// @todo livestream: Currently if the user closes the window, the camera is deactivated and it doesn't turn back on unless the broadcast is restarted.  We could allow the camera to be reactivated though.
			BroadcastStatusWindow->SetOnWindowClosed( 
				FOnWindowClosed::CreateStatic( []( const TSharedRef<SWindow>& Window, FEditorLiveStreaming* This ) 
				{
					// User closed the broadcast status window, so go ahead and shut down the web cam
					This->LiveStreamer->StopWebCam();
					This->BroadcastStatusWindow.Reset();
					This->WebCamDynamicImageBrush.Reset();
				}, 
				this ) );

			BroadcastStatusWindow->SetContent( 
				SNew( SImage )
					.Image( WebCamDynamicImageBrush.Get() )
			);

			FSlateApplication::Get().AddWindowAsNativeChild(
				BroadcastStatusWindow.ToSharedRef(), 
				RootWindow.ToSharedRef() );
		}

	}
	else if( Status.StatusType == FLiveStreamingStatus::EStatusType::WebCamTextureNotReady )
	{
		CloseBroadcastStatusWindow();
	}
}


void FEditorLiveStreaming::CloseBroadcastStatusWindow()
{
	if( BroadcastStatusWindow.IsValid() )
	{
		FSlateApplication::Get().DestroyWindowImmediately( BroadcastStatusWindow.ToSharedRef() );
		BroadcastStatusWindow.Reset();
		WebCamDynamicImageBrush.Reset();
	}
}


void FEditorLiveStreaming::OnChatMessage( const FText& UserName, const FText& ChatMessage )
{
	// @todo livestream: Currently no chat UI is supported in the editor
}


#undef LOCTEXT_NAMESPACE
