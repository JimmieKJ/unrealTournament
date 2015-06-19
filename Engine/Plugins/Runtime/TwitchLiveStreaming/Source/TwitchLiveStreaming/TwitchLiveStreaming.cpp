// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TwitchLiveStreamingModule.h"
#include "TwitchLiveStreaming.h"
#include "TwitchProjectSettings.h"
#include "ModuleManager.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Runtime/Core/Public/Stats/Stats2.h"
#include "ISettingsModule.h"


DEFINE_LOG_CATEGORY_STATIC( LogTwitch, Log, All );


#if WITH_TWITCH

IMPLEMENT_MODULE( FTwitchLiveStreaming, TwitchLiveStreaming )
DECLARE_MEMORY_STAT( TEXT( "Twitch Memory" ), STAT_TwitchMemory, STATGROUP_Memory );

#define LOCTEXT_NAMESPACE "TwitchPlugin"


void FTwitchLiveStreaming::StartupModule()
{
	TwitchDLLHandle = nullptr;

	TwitchSetTraceLevel = nullptr;
	TwitchInit = nullptr;
	TwitchShutdown = nullptr;
	TwitchPollTasks = nullptr;
	TwitchRequestAuthToken = nullptr;
	TwitchLogin = nullptr;
	TwitchErrorToString = nullptr;
	TwitchGetIngestServers = nullptr;
	TwitchFreeIngestList = nullptr;
	TwitchStart = nullptr;
	TwitchStop = nullptr;
	TwitchGetMaxResolution = nullptr;
	TwitchGetDefaultParams = nullptr;
	TwitchSubmitVideoFrame = nullptr;
	TwitchFreeGameLiveStreamList = nullptr;
	TwitchGetGameLiveStreams = nullptr;

	TwitchWebCamInit = nullptr;
	TwitchWebCamShutdown = nullptr;
	TwitchWebCamStart = nullptr;
	TwitchWebCamStop = nullptr;
	TwitchWebCamFlushEvents = nullptr;
	TwitchWebCamIsFrameAvailable = nullptr;
	TwitchWebCamGetFrame = nullptr;
	
	TwitchChatInit = nullptr;
	TwitchChatShutdown = nullptr;
	TwitchChatConnect = nullptr;
	TwitchChatDisconnect = nullptr;
	TwitchChatSendMessage = nullptr;
	TwitchChatFlushEvents = nullptr;

	TwitchState = ETwitchState::Uninitialized;
	BroadcastState = EBroadcastState::Idle;
	bWantsToBroadcastNow = false;

	bWantsWebCamNow = false;
	WebCamState = EWebCamState::Uninitialized;
	WebCamDeviceIndex = INDEX_NONE;
	WebCamCapabilityIndex = INDEX_NONE;
	WebCamVideoBufferWidth = 0;
	WebCamVideoBufferHeight = 0;
	bIsWebCamTextureFlippedVertically = false;
	WebCamTexture = nullptr;

	bWantsChatEnabled = false;
	ChatState = EChatState::Uninitialized;

	FMemory::Memzero( VideoBuffers );

	// @todo twitch: Sometimes crashes on exit after streaming in the editor on Windows 8.1
	// @todo twitch urgent: Authorization of end-user games is having problems (bad auth token error)
	// @todo twitch: Frames are not pushed while in a modal loop.  Need to register to get called from Slate modal loop in FSlateApplication::Tick() (even if modal == true).  We need to call BroadcastEditorVideoFrame() there, basically.
	// @todo twitch: Twitch sometimes doesn't stream video on the feed, even though the engine appears to be sending frame successfully.  Restarting usually fixes it.
	// @todo twitch: Add analytics information for live streaming
	// @todo twitch: Editor: Display list of available Unreal Engine live streams in the editor GUI
	// @todo twitch: Make sure that documentation includes: "-TwitchDebug" arg, console commands, BP interface, C++ interface, editor vs game
	// @todo twitch: Editor: Add buttons for audio volume, muting, pausing stream, commercials, editing channel info, etc.
	// @todo twitch: Add C++/blueprint API for audio, pausing, commercials, channel info, etc.
	// @todo twitch: On iOS, use TTV_SubmitCVPixelBuffer() to send frames
	// @todo twitch: Use TTV_SetStreamInfo to set our game's name, etc. (These can be set in the Twitch.tv web UI too)
	// @todo twitch: Use TTV_GetStreamInfo to get at current viewer count, and TTV_GetUserInfo to get the user's display name!  Also, channel info...
	// @todo twitch: Use TTV_PauseVideo() to pause display (SDK will generate frames instead), and use TTV_SetVolume() to mute/unmute! (submitting new frames will unpause video automatically)
	// @todo twitch: Use TTV_RunCommercial() to go to break! (needs scope permission bit)
	// @todo twitch: Check out meta-data APIs (SendActionMetaData, etc) (https://github.com/twitchtv/sdk-dist/wiki/Sending-Metadata, https://github.com/twitchtv/sdk-dist/wiki/Meta-data-API-v1-Event-Schema)
	// @todo twitch: Enable this plugin by default
	// @todo twitch: Implement MacOS and iOS support
	// @todo twitch: Editor: Need hyperlink to my stream from editor!
	// @todo twitch: Editor: Show audio microphone input level, so you can be sure you are not muted
	// @todo twitch: Editor: Ideally we want a "zoom in" feature to use during editor live streams

	// Register our custom project settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if( SettingsModule != nullptr )
	{
		SettingsModule->RegisterSettings(
			"Project", "Plugins", "Twitch",
			LOCTEXT( "TwitchSettingsName", "Twitch" ),
			LOCTEXT( "TwitchSettingsDescription", "Twitch game broadcasting settings" ),
			GetMutableDefault< UTwitchProjectSettings >() );
	}

	// Register as a modular feature
	IModularFeatures::Get().RegisterModularFeature( TEXT( "LiveStreaming" ), this );
}


void FTwitchLiveStreaming::ShutdownModule()
{
	// Unregister our feature
	IModularFeatures::Get().UnregisterModularFeature( TEXT( "LiveStreaming" ), this );

	// Unregister custom project settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if( SettingsModule != nullptr )
	{
		SettingsModule->UnregisterSettings( "Project", "Plugins", "Twitch" );
	}

	if( TwitchState != ETwitchState::Uninitialized )
	{
		// No longer safe to run callback functions, as we could be shutting down
		OnStatusChangedEvent.Clear();
		OnChatMessageEvent.Clear();

		// Turn off Twitch chat system
		if( ChatState != EChatState::Uninitialized )
		{
			UE_LOG( LogTwitch, Display, TEXT( "Shutting down Twitch chat system" ) );
			const TTV_ErrorCode TwitchErrorCode = TwitchChatShutdown( nullptr, nullptr );
			if( !TTV_SUCCEEDED( TwitchErrorCode ) )
			{
				const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
				UE_LOG( LogTwitch, Warning, TEXT( "An error occured while shutting down Twitch's chat system.\n\nError: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );
			}
			ChatState = EChatState::Uninitialized;
		}

		// Turn off Twitch web cam system
		if( WebCamState != EWebCamState::Uninitialized )
		{
			UE_LOG( LogTwitch, Display, TEXT( "Shutting down Twitch web cam system") );
			const TTV_ErrorCode TwitchErrorCode = TwitchWebCamShutdown( nullptr, nullptr );
			if( !TTV_SUCCEEDED( TwitchErrorCode ) )
			{
				const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
				UE_LOG( LogTwitch, Warning, TEXT( "An error occured while shutting down Twitch's web cam system.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
			}
			WebCamState = EWebCamState::Uninitialized;
		}

		// Release the web cam texture
		const bool bReleaseResourceToo = false;	// No need to release on shutdown, it will free up during normal GC phase
		ReleaseWebCamTexture( bReleaseResourceToo );

		// We are no longer broadcasting
		if( BroadcastState != EBroadcastState::Idle )
		{
			// Broadcast will be forcibly stopped by Twitch when we shutdown below.  We'll release our graphics resources afterwards.
			BroadcastState = EBroadcastState::Idle;
		}

		if( TwitchState != ETwitchState::Uninitialized && TwitchState != ETwitchState::DLLLoaded )
		{
			// Turn off Twitch
			UE_LOG( LogTwitch, Display, TEXT( "Shutting down Twitch SDK") );
			const TTV_ErrorCode TwitchErrorCode = TwitchShutdown();
			if( !TTV_SUCCEEDED( TwitchErrorCode ) )
			{
				const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
				UE_LOG( LogTwitch, Warning, TEXT( "An error occured while shutting down Twitch.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
			}

			TwitchState = ETwitchState::DLLLoaded;
		}

		// Clean up video buffers
		for( uint32 VideoBufferIndex = 0; VideoBufferIndex < TwitchVideoBufferCount; ++VideoBufferIndex )
		{
			delete[] VideoBuffers[ VideoBufferIndex ];
			VideoBuffers[ VideoBufferIndex ] = nullptr;
		}
		AvailableVideoBuffers.Reset();

		if( TwitchState == ETwitchState::DLLLoaded )
		{
			check( TwitchDLLHandle != nullptr );

			// Release the DLL
			UE_LOG( LogTwitch, Display, TEXT( "Unloading Twitch SDK DLL") );
			FPlatformProcess::FreeDllHandle( TwitchDLLHandle );
			TwitchDLLHandle = nullptr;
		}

		TwitchState = ETwitchState::Uninitialized;
	}
}


void FTwitchLiveStreaming::InitOnDemand()
{
	if( TwitchState == ETwitchState::Uninitialized )
	{
		// Load the Twitch SDK dll
		// @todo twitch: This all needs to be handled differently for iOS (static linkage)
		FString TwitchDLLFolder( FPaths::Combine( 
			*FPaths::EngineDir(), 
			TEXT("Binaries/ThirdParty/NotForLicensees/Twitch/Twitch-6.17/"),	// Check the "NotForLicensees" folder first
			FPlatformProcess::GetBinariesSubdirectory() ) );
		if( !IFileManager::Get().DirectoryExists( *TwitchDLLFolder ) )
		{
			TwitchDLLFolder = FPaths::Combine( 
				*FPaths::EngineDir(), 
				TEXT("Binaries/ThirdParty/Twitch/Twitch-6.17/"),	// Check the normal folder
				FPlatformProcess::GetBinariesSubdirectory() );
		}


		// Load the SDK DLL
		LoadTwitchSDK( TwitchDLLFolder );

		if( TwitchState == ETwitchState::DLLLoaded )
		{
			// Initialize the Twitch SDK
			InitTwitch( TwitchDLLFolder );
		}
	}
}



void FTwitchLiveStreaming::LoadTwitchSDK( const FString& TwitchDLLFolder )
{
	check( TwitchState == ETwitchState::Uninitialized );

	{
		// Make sure Twitch SDK DLL can find it's statically dependent DLLs
		FPlatformProcess::PushDllDirectory( *TwitchDLLFolder );

		const FString TwitchDLLFile( FPaths::Combine( 
			*TwitchDLLFolder, 
#if PLATFORM_MAC
			// Mac dylib
			TEXT("libsdk-mac-dynamic.dylib"),
#else
			// Windows DLL
			*FString::Printf( TEXT("twitchsdk_%s_%s.dll"),
				PLATFORM_64BITS ? TEXT( "64" ) : TEXT( "32" ),
	#if UE_BUILD_DEBUG && !defined(NDEBUG)	// Use !defined(NDEBUG) to check to see if we actually are linking with Debug third party libraries (bDebugBuildsActuallyUseDebugCRT)
				TEXT( "Debug" )
	#else
				TEXT( "Release" )
	#endif
#endif
				) ) );
		TwitchDLLHandle = FPlatformProcess::GetDllHandle( *TwitchDLLFile );

		FPlatformProcess::PopDllDirectory( *TwitchDLLFolder );
	}
 

	if( TwitchDLLHandle != nullptr )
	{
		void* LambdaDLLHandle = TwitchDLLHandle;
		
		// Helper function to load DLL exports and report warnings
		auto GetTwitchDllExport = [ &LambdaDLLHandle ]( const TCHAR* FuncName ) -> void*
			{
				void* FuncPtr = FPlatformProcess::GetDllExport( LambdaDLLHandle, FuncName );
				if( FuncPtr == nullptr )
				{
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to locate the expected DLL import function '%s' in the Twitch DLL." ), *FuncName );
					FPlatformProcess::FreeDllHandle( LambdaDLLHandle );
					LambdaDLLHandle = nullptr;
				}
				return FuncPtr;
			};

		// Grab Twitch SDK function pointers
		
		TwitchSetTraceLevel = (TwitchSetTraceLevelFuncPtr)GetTwitchDllExport( TEXT( "TTV_SetTraceLevel" ) );
		TwitchInit = (TwitchInitFuncPtr)GetTwitchDllExport( TEXT( "TTV_Init" ) );
		TwitchShutdown = (TwitchShutdownFuncPtr)GetTwitchDllExport( TEXT( "TTV_Shutdown" ) );
		TwitchPollTasks = (TwitchPollTasksFuncPtr)GetTwitchDllExport( TEXT( "TTV_PollTasks" ) );
		TwitchRequestAuthToken = (TwitchRequestAuthTokenFuncPtr)GetTwitchDllExport( TEXT( "TTV_RequestAuthToken" ) );
		TwitchLogin = (TwitchLoginFuncPtr)GetTwitchDllExport( TEXT( "TTV_Login" ) );
		TwitchErrorToString = (TwitchErrorToStringFuncPtr)GetTwitchDllExport( TEXT( "TTV_ErrorToString" ) );
		TwitchGetIngestServers = (TwitchGetIngestServersFuncPtr)GetTwitchDllExport( TEXT( "TTV_GetIngestServers" ) );
		TwitchFreeIngestList = (TwitchFreeIngestListFuncPtr)GetTwitchDllExport( TEXT( "TTV_FreeIngestList" ) );
		TwitchStart = (TwitchStartFuncPtr)GetTwitchDllExport( TEXT( "TTV_Start" ) );
		TwitchStop = (TwitchStopFuncPtr)GetTwitchDllExport( TEXT( "TTV_Stop" ) );
		TwitchGetMaxResolution = (TwitchGetMaxResolutionFuncPtr)GetTwitchDllExport( TEXT( "TTV_GetMaxResolution" ) );
		TwitchGetDefaultParams = (TwitchGetDefaultParamsFuncPtr)GetTwitchDllExport( TEXT( "TTV_GetDefaultParams" ) );
		TwitchSubmitVideoFrame = (TwitchSubmitVideoFrameFuncPtr)GetTwitchDllExport( TEXT( "TTV_SubmitVideoFrame" ) );
		TwitchFreeGameLiveStreamList = (TwitchFreeGameLiveStreamListFuncPtr)GetTwitchDllExport( TEXT( "TTV_FreeGameLiveStreamList" ) );
		TwitchGetGameLiveStreams = (TwitchGetGameLiveStreamsFuncPtr)GetTwitchDllExport( TEXT( "TTV_GetGameLiveStreams" ) );

		TwitchWebCamInit = (TwitchWebCamInitFuncPtr)GetTwitchDllExport( TEXT( "TTV_WebCam_Init" ) );
		TwitchWebCamShutdown = (TwitchWebCamShutdownFuncPtr)GetTwitchDllExport( TEXT( "TTV_WebCam_Shutdown" ) );
		TwitchWebCamStart = (TwitchWebCamStartFuncPtr)GetTwitchDllExport( TEXT( "TTV_WebCam_Start" ) );
		TwitchWebCamStop = (TwitchWebCamStopFuncPtr)GetTwitchDllExport( TEXT( "TTV_WebCam_Stop" ) );
		TwitchWebCamFlushEvents = (TwitchWebCamFlushEventsFuncPtr)GetTwitchDllExport( TEXT( "TTV_WebCam_FlushEvents" ) );
		TwitchWebCamIsFrameAvailable = (TwitchWebCamIsFrameAvailableFuncPtr)GetTwitchDllExport( TEXT( "TTV_WebCam_IsFrameAvailable" ) );
		TwitchWebCamGetFrame = (TwitchWebCamGetFrameFuncPtr)GetTwitchDllExport( TEXT( "TTV_WebCam_GetFrame" ) );

		TwitchChatInit = (TwitchChatInitFuncPtr)GetTwitchDllExport( TEXT( "TTV_Chat_Init" ) );
		TwitchChatShutdown = (TwitchChatShutdownFuncPtr)GetTwitchDllExport( TEXT( "TTV_Chat_Shutdown" ) );
		TwitchChatConnect = (TwitchChatConnectFuncPtr)GetTwitchDllExport( TEXT( "TTV_Chat_Connect" ) );
		TwitchChatDisconnect = (TwitchChatDisconnectFuncPtr)GetTwitchDllExport( TEXT( "TTV_Chat_Disconnect" ) );
		TwitchChatSendMessage = (TwitchChatSendMessageFuncPtr)GetTwitchDllExport( TEXT( "TTV_Chat_SendMessage" ) );
		TwitchChatFlushEvents = (TwitchChatFlushEventsFuncPtr)GetTwitchDllExport( TEXT( "TTV_Chat_FlushEvents" ) );

		TwitchDLLHandle = LambdaDLLHandle;
	}
	else
	{
		int32 ErrorNum = FPlatformMisc::GetLastError();
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage( ErrorMsg, 1024, ErrorNum );
		UE_LOG( LogTwitch, Warning, TEXT("Failed to get Twitch DLL handle.\n\nError: %s (%d)"), ErrorMsg, ErrorNum );
	}

	if( TwitchDLLHandle != nullptr )
	{
		// OK, everything loaded!
		TwitchState = ETwitchState::DLLLoaded;
	}
}


FTwitchLiveStreaming::FTwitchProjectSettings FTwitchLiveStreaming::LoadProjectSettings()
{
	FTwitchProjectSettings Settings;
	if( GIsEditor )
	{
		// @todo twitch: When invoking PIE from the editor, streaming will use the EDITOR credentials, not the game.  I think that is OK though?

		// In the editor, we use a single Twitch application "client ID" for all sessions.
		const FString ConfigCategory( TEXT( "TwitchEditorBroadcasting" ) );

		// This is the Twitch 'client ID' for the Unreal Engine Twitch application.  This allows anyone to login to Twitch and broadcast
		// usage of the editor to their Twitch channel.  This is only used when running the editor though.  To allow your users to
		// stream gameplay to Twitch, you'll need to create your own Twitch client ID and set that in your project's settings!
		Settings.ClientID = TEXT( "nu2tppawhw58o74a2burkyc9fyuvjlr" );
		Settings.RedirectURI = TEXT( "http://localhost:6180" );	// @todo twitch: Need nicer redirect landing page

		// UserName, Password and ClientSecret are used for editor direct authentication with Twitch.  This authentication method 
		// is less secure, and requires Twitch to whitelist your application's ClientID in order for it to authenticate.  When
		// these fields are empty, a browser-based authentication will be used instead.  This is just for testing.
		
		// These are just for testing.  We don't really support this workflow yet, as there is no editor UI for prompting
		// for credentials.  Also, it's not very secure compared to the browser-based login.
		Settings.UserName = TEXT( "" );
		Settings.Password = TEXT( "" );
		Settings.ClientSecret = TEXT( "" );
	}
	else
	{
		// For the game, Twitch settings are configured inside of the game-specific project settings.  Use the editor's
		// Project Settings window to configure settings for streaming your game to Twitch!
		const auto& TwitchProjectSettings = *GetDefault< UTwitchProjectSettings >();
		Settings.ClientID = TwitchProjectSettings.ApplicationClientID;
		Settings.RedirectURI = TwitchProjectSettings.RedirectURI;
	}

	return Settings;
}


void FTwitchLiveStreaming::InitTwitch( const FString& TwitchWindowsDLLFolder )
{
	check( TwitchState == ETwitchState::DLLLoaded );

	// @todo twitch: This can be enabled to emit verbose log spew from the Twitch SDK to the IDE output window
	if( FParse::Param( FCommandLine::Get(), TEXT("TwitchDebug") ) )
	{
		TwitchSetTraceLevel( TTV_ML_DEBUG );
	}

	// Load Twitch configuration from disk
	ProjectSettings = LoadProjectSettings();

	// Setup memory allocation and deallocation overrides
	TTV_MemCallbacks TwitchMemCallbacks;
	{
		struct Local
		{
			static void* TwitchAlloc( size_t Size, size_t Alignment )
			{
				void* Result = FMemory::Malloc( Size, uint32( Alignment ) );
#if STATS
				const SIZE_T ActualSize = FMemory::GetAllocSize( Result );
				INC_DWORD_STAT_BY( STAT_TwitchMemory, ActualSize );
#endif // STATS
				return Result;
			}

			static void TwitchFree( void* Ptr )
			{
#if STATS
				const SIZE_T Size = FMemory::GetAllocSize( Ptr );
				DEC_DWORD_STAT_BY( STAT_TwitchMemory, Size );	
#endif // STATS
				FMemory::Free( Ptr );
			}

		};

		TwitchMemCallbacks.size = sizeof( TwitchMemCallbacks );
		TwitchMemCallbacks.allocCallback = &Local::TwitchAlloc;
		TwitchMemCallbacks.freeCallback = &Local::TwitchFree;
	}

	const TTV_ErrorCode TwitchErrorCode = TwitchInit( &TwitchMemCallbacks, TCHAR_TO_ANSI( *ProjectSettings.ClientID ), *TwitchWindowsDLLFolder );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Couldn't initialize Twitch SDK.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
	}
	else
	{
		UE_LOG( LogTwitch, Display, TEXT( "Twitch plugin is initialized and ready") );
		TwitchState = ETwitchState::ReadyToAuthenticate;
	}
}


void FTwitchLiveStreaming::Async_AuthenticateWithTwitchDirectly( const FString& UserName, const FString& Password, const FString& ClientSecret )
{
	check( TwitchState == ETwitchState::ReadyToAuthenticate );

	// This will be filled in by TTV_RequestAuthToken()
	FMemory::Memzero( TwitchAuthToken );

	TTV_AuthParams TwitchAuthParams;
	TwitchAuthParams.size = sizeof( TwitchAuthParams );

	ANSICHAR* UserNameAnsi = (ANSICHAR*)FMemory_Alloca( ( UserName.Len() + 1 ) * sizeof(ANSICHAR) );
	FPlatformString::Strcpy( UserNameAnsi, UserName.Len() + 1, TCHAR_TO_ANSI( *UserName ) );
	TwitchAuthParams.userName = UserNameAnsi;

	ANSICHAR* PasswordAnsi = (ANSICHAR*)FMemory_Alloca( ( Password.Len() + 1 ) * sizeof(ANSICHAR) );
	FPlatformString::Strcpy( PasswordAnsi, Password.Len() + 1, TCHAR_TO_ANSI( *Password ) );
	TwitchAuthParams.password = PasswordAnsi;

	ANSICHAR* ClientSecretAnsi = (ANSICHAR*)FMemory_Alloca( ( ClientSecret.Len() + 1 ) * sizeof(ANSICHAR) );
	FPlatformString::Strcpy( ClientSecretAnsi, ClientSecret.Len() + 1, TCHAR_TO_ANSI( *ClientSecret ) );
	TwitchAuthParams.clientSecret = ClientSecretAnsi;

	const uint32 TwitchRequestAuthTokenFlags = 
		TTV_RequestAuthToken_Broadcast |		// Permission to broadcast
		TTV_RequestAuthToken_Chat;				// Permission to chat

	// Called when TwitchRequestAuthToken finishes async work
	void* CallbackUserDataPtr = this;
	TTV_TaskCallback TwitchTaskCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( ensure( This->TwitchState == ETwitchState::WaitingForDirectAuthentication ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// If this call succeeded, we must now have valid auth token data
					if( ensure( This->TwitchAuthToken.data[0] != 0 ) )
					{
						// OK, we're authorized now.  Next we need to login.
						UE_LOG( LogTwitch, Display, TEXT( "Twitch authentication was successful (auth token '%s')"), ANSI_TO_TCHAR( This->TwitchAuthToken.data ) );
						This->PublishStatus( EStatusType::Progress, LOCTEXT( "Status_AuthenticationOK", "Successfully authenticated with Twitch" ) );
						This->TwitchState = ETwitchState::ReadyToLogin;
					}
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to authenticate with Twitch server.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
					This->PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_AuthenticationFailed", "Failed to authenticate with Twitch ({0})" ), FText::FromString( TwitchErrorString ) ) );
					This->TwitchState = ETwitchState::LoginFailure;
				}
			}
		};

	UE_LOG( LogTwitch, Display, TEXT( "Authenticating with Twitch directly as user '%s' (passing along password and client secret)"), ANSI_TO_TCHAR( TwitchAuthParams.userName ) );
	PublishStatus( EStatusType::Progress, LOCTEXT( "Status_Authenticating", "Authenticating with Twitch..." ) );

	TwitchState = ETwitchState::WaitingForDirectAuthentication;
	const TTV_ErrorCode TwitchErrorCode = TwitchRequestAuthToken( &TwitchAuthParams, TwitchRequestAuthTokenFlags, TwitchTaskCallback, CallbackUserDataPtr, &TwitchAuthToken );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to initiate authentication with Twitch server.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_AuthenticationStartFailed", "Failed to start authenticate with Twitch ({0})" ), FText::FromString( TwitchErrorString ) ) );
		TwitchState = ETwitchState::LoginFailure;
	}
}


void FTwitchLiveStreaming::Async_AuthenticateWithTwitchUsingBrowser()
{
	check( TwitchState == ETwitchState::ReadyToAuthenticate );

	UE_LOG( LogTwitch, Display, TEXT( "Launching a web browser so that the user can manually authenticate with Twitch") );
	PublishStatus( EStatusType::Progress, LOCTEXT( "Status_AuthenticatingWithBrowser", "Waiting for web browser authentication..." ) );

	// List of possible scopes can be found here:  https://github.com/justintv/Twitch-API/blob/master/authentication.md#scopes
	// @todo twitch: Allow user to login with chat access only (no broadcasting), for games that just want to participate in chat
	const FString TwitchScopes( TEXT( "user_read+channel_read+channel_editor+sdk_broadcast+chat_login" ) );		// channel_commercial+metadata_events_edit+user_blocks_edit+user_blocks_read+user_follows_edit+channel_commercial+channel_stream+channel_subscriptions+user_subscriptions+channel_check_subscription" ) );

	// Build up a URL string to give to the web browser
	const FString URLString( FString::Printf(
		TEXT( "https://api.twitch.tv/kraken/oauth2/authorize?response_type=token&client_id=%s&redirect_uri=%s&scope=%s" ),
		*ProjectSettings.ClientID,		// Client ID
		*ProjectSettings.RedirectURI,		// Redirect URI
		*TwitchScopes ) );		// Space(+)-separated list of scopes
	
	// Launch the web browser on this computer
	FString LaunchURLError;
	FPlatformProcess::LaunchURL( *URLString, TEXT( "" ), &LaunchURLError );

	if( LaunchURLError.IsEmpty() )
	{
		// OK, let's hang tight while the user logs in.  We'll check up on it by calling CheckIfBrowserLoginCompleted() every so often
		TwitchState = ETwitchState::WaitingForBrowserBasedAuthentication;

		BrowserLoginStartTime = FDateTime::UtcNow();
		LastBrowserLoginCheckTime = FDateTime::MinValue();
	}
	else
	{
		// Something went wrong, couldn't launch the browser
		UE_LOG( LogTwitch, Warning, TEXT( "Unable to launch web browser to start Twitch authentication (URL: '%s', Error: '%s')" ), *URLString, *LaunchURLError );
		PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_WebAuthenticationStartFailed", "Failed to launch web browser to authenticate with Twitch ({0})" ), FText::FromString( LaunchURLError ) ) );
		TwitchState = ETwitchState::LoginFailure;
	}
}


void FTwitchLiveStreaming::CheckIfBrowserLoginCompleted()
{
	// @todo twitch: It would be nice if we could close the browser tab after we've got the access token from it

	// How long to wait before we go looking for a window title that matches our request.  We don't want to get a stale
	// browser tab that has an old authentication token, so we need to make sure to give the browser tab enough time to open
	const float SecondsBeforeFirstLoginCheck = 0.25f;

	// How long between each check we perform.  This is just to reduce CPU usage.
	const float SecondsBetweenBrowserLoginChecks = 0.2f;

	// How long the user has to authenticate with Twitch using the web site.  They may need to interactively login at this time,
	// so we need to be a little bit generous
	const float BrowserLoginTimeoutDuration = 120.0f;

	const FDateTime CurrentTime = FDateTime::UtcNow();
	if( CurrentTime - BrowserLoginStartTime >= FTimespan::FromSeconds( SecondsBeforeFirstLoginCheck ) )
	{
		if( ( CurrentTime - LastBrowserLoginCheckTime ) >= FTimespan::FromSeconds( SecondsBetweenBrowserLoginChecks ) )
		{
			LastBrowserLoginCheckTime = CurrentTime;

			// Find the browser window we spawned which should now be titled with the redirect URL
			FString Title;
			if( FPlatformMisc::GetWindowTitleMatchingText( *ProjectSettings.RedirectURI, Title ) )
			{
				// Parse access token from the login redirect url
				FString AccessToken;
				if( FParse::Value(*Title, TEXT("#access_token="), AccessToken) && !AccessToken.IsEmpty() )
				{
					// Strip off any URL parameters and just keep the token itself
					FString AccessTokenOnly;
					if( AccessToken.Split( TEXT("&"), &AccessTokenOnly, NULL ) )
					{
						AccessToken = AccessTokenOnly;
					}

					FPlatformString::Strcpy( TwitchAuthToken.data, AccessToken.Len() + 1, TCHAR_TO_ANSI( *AccessToken ) );

					// OK, we have the auth token!
					TwitchState = ETwitchState::ReadyToLogin;
				}
				else
				{
					// Couldn't parse the window title text
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to parse the authentication token text from the scraped browser window with the redirect URI string ('%s')"), *Title );
					PublishStatus( EStatusType::Failure, LOCTEXT( "Status_WebAuthenticationParseFailed", "Authentication failed.  Couldn't find authentication token string in web URL" ) );
					TwitchState = ETwitchState::LoginFailure;
				}
			}
			else
			{
				// Couldn't find a browser window with a title we were expecting.  
				if( ( CurrentTime - BrowserLoginStartTime ) >= FTimespan::FromSeconds( BrowserLoginTimeoutDuration ) )
				{
					// Too much time has passed.  We need to abort the login
					UE_LOG( LogTwitch, Warning, TEXT( "We timed out after %.2f seconds waiting for user to complete browser-based authentication with Twitch"), BrowserLoginTimeoutDuration );
					PublishStatus( EStatusType::Failure, LOCTEXT( "Status_WebAuthenticationTimedOut", "Twitch web authentication timed out.  Please restart your broadcast to try again." ) );
					TwitchState = ETwitchState::LoginFailure;
				}
				else
				{
					// We'll try again later.				
				}
			}
		}
	}
}


void FTwitchLiveStreaming::Async_LoginToTwitch()
{
	check( TwitchState == ETwitchState::ReadyToLogin );

	// Calling TTV_Login() will fill the TwitchChannelInfo structure with information about the Twitch channel
	FMemory::Memzero( TwitchChannelInfo );	// @todo twitch: We should expose this to the live stream API
	TwitchChannelInfo.size = sizeof( TwitchChannelInfo );

	// Called when TwitchLogin finishes async work
	void* CallbackUserDataPtr = this;
	TTV_TaskCallback TwitchTaskCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( ensure( This->TwitchState == ETwitchState::WaitingForLogin ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// OK, we're now logged in!
					UE_LOG( LogTwitch, Display, TEXT( "Logged into Twitch successfully!") );
					UE_LOG( LogTwitch, Display, TEXT( "Connected to Twitch channel '%s' (Name:%s, URL: %s)"), ANSI_TO_TCHAR( This->TwitchChannelInfo.displayName ), ANSI_TO_TCHAR( This->TwitchChannelInfo.name ), ANSI_TO_TCHAR( This->TwitchChannelInfo.channelUrl ) );
					This->PublishStatus( EStatusType::Progress, LOCTEXT( "Status_LoginOK", "Logged into Twitch successfully!" ) );

					This->TwitchState = ETwitchState::LoggedIn;
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to login to Twitch.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
					This->PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_LoginFailed", "Failed to login to Twitch ({0})" ), FText::FromString( TwitchErrorString ) ) );
					This->TwitchState = ETwitchState::LoginFailure;
				}
			}
		};

	UE_LOG( LogTwitch, Display, TEXT( "Logging into Twitch (auth token '%s')..."), ANSI_TO_TCHAR( TwitchAuthToken.data ) );
	PublishStatus( EStatusType::Progress, LOCTEXT( "Status_LoggingIn", "Logging into Twitch server..." ) );

	TwitchState = ETwitchState::WaitingForLogin;
	const TTV_ErrorCode TwitchErrorCode = TwitchLogin( &TwitchAuthToken, TwitchTaskCallback, CallbackUserDataPtr, &TwitchChannelInfo );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to initiate login to Twitch.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_LoginStartFailed", "Failed to start logging into Twitch ({0})" ), FText::FromString( TwitchErrorString ) ) );
		TwitchState = ETwitchState::LoginFailure;
	}
}



void FTwitchLiveStreaming::Async_GetIngestServers()
{
	check( TwitchState == ETwitchState::LoggedIn );
	check( BroadcastState == EBroadcastState::Idle );

	// This will be filled in by calling TTV_GetIngestServers()
	FMemory::Memzero( TwitchIngestList );

	// Called when TwitchGetIngestServers finishes async work
	void* CallbackUserDataPtr = this;
	TTV_TaskCallback TwitchTaskCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( ensure( This->BroadcastState == EBroadcastState::WaitingForGetIngestServers ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// OK we have our list of servers.  We can now start broadcasting when ready.
					UE_LOG( LogTwitch, Display, TEXT( "Found %i Twitch ingest servers:" ), This->TwitchIngestList.ingestCount );
					for( uint32 ServerIndex = 0; ServerIndex < This->TwitchIngestList.ingestCount; ++ServerIndex )
					{
						const auto& ServerInfo = This->TwitchIngestList.ingestList[ ServerIndex ];
						UE_LOG( LogTwitch, Display, TEXT( "   %i.  %s  (URL:%s, default:%s)"), ServerIndex, ANSI_TO_TCHAR( ServerInfo.serverName ), ANSI_TO_TCHAR( ServerInfo.serverUrl ), ServerInfo.defaultServer ? TEXT( "yes" ) : TEXT( "no" ) );
					}
					This->BroadcastState = EBroadcastState::ReadyToBroadcast;
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to query list of available ingest servers.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
					This->PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_QueryServerListFailed", "Failed to query server list ({0})" ), FText::FromString( TwitchErrorString ) ) );
					This->BroadcastState = EBroadcastState::BroadcastingFailure;
				}
			}
		};

	UE_LOG( LogTwitch, Display, TEXT( "Querying available Twitch ingest servers for broadcasting...") );
	PublishStatus( EStatusType::Progress, LOCTEXT( "Status_QueryingServers", "Querying Twitch server list..." ) );
	BroadcastState = EBroadcastState::WaitingForGetIngestServers;
	const TTV_ErrorCode TwitchErrorCode = TwitchGetIngestServers( &TwitchAuthToken, TwitchTaskCallback, CallbackUserDataPtr, &TwitchIngestList );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to initiate query for list of available ingest servers.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_QueryServerListStartFailed", "Failed to start querying server list ({0})" ), FText::FromString( TwitchErrorString ) ) );
		BroadcastState = EBroadcastState::BroadcastingFailure;
	}
}


void FTwitchLiveStreaming::Async_StartBroadcasting()
{
	check( BroadcastState == EBroadcastState::ReadyToBroadcast );

	// Pick an ingest server
	TTV_IngestServer SelectedTwitchIngestServer;
	FMemory::Memzero( SelectedTwitchIngestServer );
	{
		// Select the default ingest server recommended by the Twitch SDK
		// @todo twitch: We might want to run an 'ingest test' first.  This will help with bitrate selection too, apparently
		bool bFoundDefaultServer = false;
		for( uint32 ServerIndex = 0; ServerIndex < TwitchIngestList.ingestCount; ++ServerIndex )
		{
			const TTV_IngestServer& CurrentTwitchIngestServer = TwitchIngestList.ingestList[ ServerIndex ];
			if( CurrentTwitchIngestServer.defaultServer == true )
			{
				// Use the default server
				SelectedTwitchIngestServer = CurrentTwitchIngestServer;
				bFoundDefaultServer = true;
				break;
			}
		}
		if( bFoundDefaultServer )
		{
			UE_LOG( LogTwitch, Display, TEXT( "Selected Twitch ingest server '%s' for broadcasting"), ANSI_TO_TCHAR( SelectedTwitchIngestServer.serverName ) );
		}
		else
		{
			UE_LOG( LogTwitch, Warning, TEXT( "Failed to start broadcasting because we couldn't find a default ingest server to use") );
			PublishStatus( EStatusType::Failure, LOCTEXT( "Status_FailedToFindServer", "Couldn't find a valid Twitch server to stream to" ) );
			BroadcastState = EBroadcastState::BroadcastingFailure;
		}
	}


	UE_LOG( LogTwitch, Display, TEXT( "Twitch broadcast configured for %i x %i resolution"), BroadcastConfig.VideoBufferWidth, BroadcastConfig.VideoBufferHeight );


	// Check with Twitch to see if the configured resolution is too high, and warn the user if so
	{
		// @todo twitch: Customize these properly or make data driven.  Though, we don't really use this for much of anything yet, other than a warning log message

		const uint32 TwitchMaxKbps = 1024;	// Maximum bitrate supported (this should be determined by running the ingest tester for a given ingest server)
		const float TwitchBitsPerPixel = 0.2f;	// The bits per pixel used in the final encoded video. A fast motion game (e.g. first person shooter) required more bits per pixel of encoded video avoid compression artifacting. Use 0.1 for an average motion game. For games without too many fast changes in the scene, you could use a value below 0.1 but not much. For fast moving games with lots of scene changes a value as high as  0.2 would be appropriate.
		const float TwitchAspectRatio = 1.6f;	// The aspect ratio of the video which we'll use for calculating width and height

		uint MaximumWidth = 0;
		uint MaximumHeight = 0;
		if( BroadcastState == EBroadcastState::ReadyToBroadcast )
		{
			const TTV_ErrorCode TwitchErrorCode = TwitchGetMaxResolution(
				TwitchMaxKbps,
				BroadcastConfig.FramesPerSecond,
				TwitchBitsPerPixel,
				TwitchAspectRatio,
				&MaximumWidth,
				&MaximumHeight );
			if( !TTV_SUCCEEDED( TwitchErrorCode ) )
			{
				const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
				UE_LOG( LogTwitch, Warning, TEXT( "Failed to determine maximum resolution for Twitch video stream.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
				BroadcastState = EBroadcastState::BroadcastingFailure;
			}

			UE_LOG( LogTwitch, Display, TEXT( "Twitch suggested a maximum streaming resolution of %i x %i for our parameters"), MaximumWidth, MaximumHeight );
		}

		if( (int)MaximumWidth < BroadcastConfig.VideoBufferWidth || (int)MaximumHeight < BroadcastConfig.VideoBufferHeight )
		{
			UE_LOG( LogTwitch, Warning, TEXT( "Twitch warns that configured resolution (%i x %i) is too high for the configured bit rate and frame rate, suggests using %i x %i instead."), BroadcastConfig.VideoBufferWidth, BroadcastConfig.VideoBufferHeight, MaximumWidth, MaximumHeight );
		}
	}

	// Start streaming
	if( BroadcastState == EBroadcastState::ReadyToBroadcast )
	{
		// @todo twitch: Make sure these settings are all filled in!
		TTV_VideoParams TwitchVideoParams;
		FMemory::Memzero( TwitchVideoParams );
		TwitchVideoParams.size = sizeof( TwitchVideoParams );
		TwitchVideoParams.outputWidth = BroadcastConfig.VideoBufferWidth;
		TwitchVideoParams.outputHeight = BroadcastConfig.VideoBufferHeight;
		TwitchVideoParams.targetFps = BroadcastConfig.FramesPerSecond;	// @todo twitch: We can save CPU cycles by not emitting frames when Twitch is behind (maybe compare buffer pointer to see if same buffer as last time?)

		// Ask Twitch to fill in the rest of the video options based on the width, height and target FPS
		TwitchGetDefaultParams( &TwitchVideoParams );

		// Set our pixel format
		switch( BroadcastConfig.PixelFormat )
		{
			case FBroadcastConfig::EBroadcastPixelFormat::B8G8R8A8:
				TwitchVideoParams.pixelFormat = TTV_PF_BGRA;
				break;

			case FBroadcastConfig::EBroadcastPixelFormat::R8G8B8A8:
			default:
				TwitchVideoParams.pixelFormat = TTV_PF_RGBA;
				break;

			case FBroadcastConfig::EBroadcastPixelFormat::A8R8G8B8:
				TwitchVideoParams.pixelFormat = TTV_PF_ARGB;
				break;

			case FBroadcastConfig::EBroadcastPixelFormat::A8B8G8R8:
				TwitchVideoParams.pixelFormat = TTV_PF_ABGR;
				break;
		}

		TTV_AudioParams TwitchAudioParams;
		FMemory::Memzero( TwitchAudioParams );
		TwitchAudioParams.size = sizeof( TwitchAudioParams );
		TwitchAudioParams.audioEnabled = BroadcastConfig.bCaptureAudioFromComputer || BroadcastConfig.bCaptureAudioFromMicrophone;
		TwitchAudioParams.enableMicCapture = BroadcastConfig.bCaptureAudioFromMicrophone;
		TwitchAudioParams.enablePlaybackCapture = BroadcastConfig.bCaptureAudioFromComputer;
		TwitchAudioParams.enablePassthroughAudio = false;	// @todo twitch: Usually only needed on iOS platform

		const uint32 TwitchStartFlags = 0;	// @todo twitch: Also supports: TTV_Start_BandwidthTest

		// @todo twitch: This call may fail on Mac if SoundFlower is not installed

		// Called when TwitchStart finishes async work
		void* CallbackUserDataPtr = this;
		TTV_TaskCallback TwitchTaskCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
			{
				FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

				// Make sure we're still in the state that we were expecting
				if( ensure( This->BroadcastState == EBroadcastState::WaitingToStartBroadcasting ) )
				{
					if( TTV_SUCCEEDED( TwitchErrorCode ) )
					{
						// OK we are now LIVE!
						UE_LOG( LogTwitch, Display, TEXT( "Twitch broadcast has started.  You are now LIVE!" ) );
						This->BroadcastState = EBroadcastState::Broadcasting;
						This->PublishStatus( EStatusType::BroadcastStarted, LOCTEXT( "Status_BroadcastStarted", "Twitch broadcast started.  You are now LIVE!" ) );

						// Get our video buffers ready
						This->AvailableVideoBuffers.Reset();
						for( uint32 VideoBufferIndex = 0; VideoBufferIndex < This->TwitchVideoBufferCount; ++VideoBufferIndex )
						{
							// Allocate buffer (width * height * bytes per pixel)
							This->VideoBuffers[ VideoBufferIndex ] = new uint8[ This->BroadcastConfig.VideoBufferWidth * This->BroadcastConfig.VideoBufferHeight * 4 ];
							This->AvailableVideoBuffers.Add( This->VideoBuffers[ VideoBufferIndex ] );
						}
					}
					else
					{
						const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
						UE_LOG( LogTwitch, Warning, TEXT( "Failed to start broadcasting to Twitch.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
						This->PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_BroadcastFailed", "Twitch broadcasting failed ({0})" ), FText::FromString( TwitchErrorString ) ) );
						This->BroadcastState = EBroadcastState::BroadcastingFailure;
					}
				}
			};

		UE_LOG( LogTwitch, Display, TEXT( "Starting Twitch broadcast..." ) );
		PublishStatus( EStatusType::Progress, LOCTEXT( "Status_StartingBroadcast", "Starting broadcast..." ) );

		BroadcastState = EBroadcastState::WaitingToStartBroadcasting;
		const TTV_ErrorCode TwitchErrorCode = TwitchStart( &TwitchVideoParams, &TwitchAudioParams, &SelectedTwitchIngestServer, TwitchStartFlags, TwitchTaskCallback, CallbackUserDataPtr );
		if( !TTV_SUCCEEDED( TwitchErrorCode ) )
		{
			const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
			UE_LOG( LogTwitch, Warning, TEXT( "Failed to initiate start of broadcasting to Twitch.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
			PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_BroadcastStartFailed", "Failed to start broadcast ({0})" ), FText::FromString( TwitchErrorString ) ) );
			BroadcastState = EBroadcastState::BroadcastingFailure;
		}
	}
}



void FTwitchLiveStreaming::Async_StopBroadcasting()
{
	check( BroadcastState == EBroadcastState::Broadcasting );

	// Called when TwitchStop finishes async work
	void* CallbackUserDataPtr = this;
	TTV_TaskCallback TwitchTaskCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( ensure( This->BroadcastState == EBroadcastState::WaitingToStopBroadcasting ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// Broadcasting has stopped.  We can start again whenever we want.
					UE_LOG( LogTwitch, Display, TEXT( "Twitch broadcast has stopped.  We are longer streaming live." ) );
					This->PublishStatus( EStatusType::BroadcastStopped, LOCTEXT( "Status_BroadcastStopped", "Broadcasting has ended." ) );
					This->BroadcastState = EBroadcastState::ReadyToBroadcast;

					// Clean up video buffers
					This->AvailableVideoBuffers.Reset();
					for( uint32 VideoBufferIndex = 0; VideoBufferIndex < This->TwitchVideoBufferCount; ++VideoBufferIndex )
					{
						delete[] This->VideoBuffers[ VideoBufferIndex ];
						This->VideoBuffers[ VideoBufferIndex ] = nullptr;
					}
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to stop broadcasting to Twitch.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
					This->PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_BroadcastStopFailed", "Failed while stopping Twitch broadcast ({0})" ), FText::FromString( TwitchErrorString ) ) );
					This->BroadcastState = EBroadcastState::BroadcastingFailure;
				}
			}
		};

	UE_LOG( LogTwitch, Display, TEXT( "Halting Twitch broadcast..." ) );
	PublishStatus( EStatusType::Progress, LOCTEXT( "Status_StoppingBroadcast", "Stopping Twitch broadcast..." ) );

	BroadcastState = EBroadcastState::WaitingToStopBroadcasting;
	const TTV_ErrorCode TwitchErrorCode = TwitchStop( TwitchTaskCallback, CallbackUserDataPtr );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to initiative stop of broadcasting to Twitch.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		PublishStatus( EStatusType::Failure, FText::Format( LOCTEXT( "Status_BroadcastStopCallFailed", "Error while stopping Twitch broadcast ({0})" ), FText::FromString( TwitchErrorString ) ) );
		BroadcastState = EBroadcastState::BroadcastingFailure;
	}
}


void FTwitchLiveStreaming::Async_SendVideoFrame( const FColor* VideoFrameBuffer )
{
	check( BroadcastState == EBroadcastState::Broadcasting );

	// Called when TwitchStop finishes async work
	void* CallbackUserDataPtr = this;
	TTV_BufferUnlockCallback TwitchBufferUnlockCallback = []( const uint8_t* Buffer, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// This buffer has become available to use again!
			This->AvailableVideoBuffers.Add( const_cast<uint8*>( Buffer ) );
		};

	// We should always have an available buffer ready.  Twitch guarantees that it never uses more than three at a time.
	uint8* TwitchFrameBuffer = AvailableVideoBuffers.Pop();

	// Fill the buffer
	FMemory::Memcpy( TwitchFrameBuffer, VideoFrameBuffer, BroadcastConfig.VideoBufferWidth * BroadcastConfig.VideoBufferHeight * 4 );

	const TTV_ErrorCode TwitchErrorCode = TwitchSubmitVideoFrame( TwitchFrameBuffer, TwitchBufferUnlockCallback, CallbackUserDataPtr );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		// NOTE: Even if sending the buffer fails for some reason, we continue to stream.
		// @todo twitch: Any specific error codes for this that we should catch and totally abort the stream?

		// NOTE: This warning is set as 'verbose' because we don't want to kill performance by spamming a transmit error every frame
		UE_LOG( LogTwitch, Verbose, TEXT( "Unable to transmit video buffer to Twitch.\n\nError: %s (%d)"), UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ), (int32)TwitchErrorCode );
	}
}



void FTwitchLiveStreaming::PublishStatus( const EStatusType Type, const FText& Text )
{
	if( OnStatusChangedEvent.IsBound() )
	{
		FLiveStreamingStatus NewStatus;
		NewStatus.CustomStatusDescription = Text;
		NewStatus.StatusType = Type;

		OnStatusChangedEvent.Broadcast( NewStatus );
	}
}



ILiveStreamingService::FOnStatusChanged& FTwitchLiveStreaming::OnStatusChanged()
{
	return OnStatusChangedEvent;
}


void FTwitchLiveStreaming::StartBroadcasting( const FBroadcastConfig& Config )
{
	InitOnDemand();

	if( !IsBroadcasting() )
	{
		bWantsToBroadcastNow = true;
		BroadcastConfig = Config;

		// Tick right away to initiate the login
		this->Tick( 0.0f );
	}
}


void FTwitchLiveStreaming::StopBroadcasting()
{
	InitOnDemand();

	if( IsBroadcasting() )
	{
		bWantsToBroadcastNow = false;

		// Tick right away to halt the broadcast
		this->Tick( 0.0f );
	}
}


bool FTwitchLiveStreaming::IsBroadcasting() const
{
	return bWantsToBroadcastNow;
}


bool FTwitchLiveStreaming::IsReadyForVideoFrames() const
{
	return BroadcastState == EBroadcastState::Broadcasting;
}


void FTwitchLiveStreaming::MakeValidVideoBufferResolution( int& VideoBufferWidth, int& VideoBufferHeight ) const
{
	struct Local
	{
		/** Given a number, rounds the number of to the next specified multiple */
		static int RoundUpToNextMultipleOf( int Value, int Multiple )
		{ 
			int Result = Value;
			if( Multiple != 0 )
			{ 
				int Remainder = Value % Multiple;
				if( Remainder != 0 )
				{
					Result = Value + Multiple - Remainder;
				}
			}
			return Result;
		}
	};

	// Twitch requires video buffer width to be a multiple of 32, and video buffer height to be a multiple of 16
	// NOTE: This might affect the aspect ratio of the resolution.  The caller will need to compensate.
	VideoBufferWidth = Local::RoundUpToNextMultipleOf( VideoBufferWidth, 32 );
	VideoBufferHeight = Local::RoundUpToNextMultipleOf( VideoBufferHeight, 16 );
}


void FTwitchLiveStreaming::QueryBroadcastConfig( FBroadcastConfig& OutBroadcastConfig ) const
{
	if( ensure( IsBroadcasting() ) )
	{
		OutBroadcastConfig = BroadcastConfig;
	}
}

	
void FTwitchLiveStreaming::PushVideoFrame( const FColor* VideoFrameBuffer )
{
	if( ensure( BroadcastState == EBroadcastState::Broadcasting ) )
	{
		Async_SendVideoFrame( VideoFrameBuffer );
	}
}


void FTwitchLiveStreaming::StartWebCam( const FWebCamConfig& Config )
{
	InitOnDemand();
	if( !IsWebCamEnabled() )
	{
		bWantsWebCamNow = true;
		WebCamConfig = Config;

		// Tick right away to initiate the web cam setup
		this->Tick( 0.0f );
	}
}


void FTwitchLiveStreaming::StopWebCam()
{
	InitOnDemand();
	if( IsWebCamEnabled() )
	{
		bWantsWebCamNow = false;

		// Tick right away to halt the web cam
		this->Tick( 0.0f );
	}
}


bool FTwitchLiveStreaming::IsWebCamEnabled() const
{
	return bWantsWebCamNow;
}


UTexture2D* FTwitchLiveStreaming::GetWebCamTexture( bool& bIsImageFlippedHorizontally, bool& bIsImageFlippedVertically )
{
	bIsImageFlippedHorizontally = false;	// Twitch never flips a web cam image horizontally
	bIsImageFlippedVertically = bIsWebCamTextureFlippedVertically;
	return WebCamTexture;
}


ILiveStreamingService::FOnChatMessage& FTwitchLiveStreaming::OnChatMessage()
{
	return OnChatMessageEvent;
}


void FTwitchLiveStreaming::ConnectToChat()
{
	InitOnDemand();
	if( !IsChatEnabled() )
	{
		bWantsChatEnabled = true;

		// Tick right away to initiate the chat connect
		this->Tick( 0.0f );
	}
}


void FTwitchLiveStreaming::DisconnectFromChat()
{
	InitOnDemand();
	if( IsChatEnabled() )
	{
		bWantsChatEnabled = false;

		// Tick right away to halt the chat
		this->Tick( 0.0f );
	}
}


bool FTwitchLiveStreaming::IsChatEnabled() const
{
	return bWantsChatEnabled;
}


void FTwitchLiveStreaming::SendChatMessage( const FString& ChatMessage )
{
	if( ensure( IsChatEnabled() && ChatState == EChatState::Connected ) )
	{
		Async_SendChatMessage( ChatMessage );
	}
}


void FTwitchLiveStreaming::QueryLiveStreams( const FString& GameName, FQueryLiveStreamsCallback CompletionCallback ) 
{
	InitOnDemand();

	// @todo twitch urgent: Don't we need to be logged in first?  Need to do async login as part of this?  Or expose a Login API with status events
	Async_GetGameLiveStreams( GameName, CompletionCallback );
}



void FTwitchLiveStreaming::Tick( float DeltaTime )
{
	check( IsTickable() );

	// To be able to broadcast video or chat, we need to get authorization from the Twitch server and complete a successful login
	if( bWantsToBroadcastNow || bWantsChatEnabled )
	{
		// User wants to be broadcasting.  Let's get it going!
		if( TwitchState == ETwitchState::ReadyToAuthenticate )
		{
			// If we have a user name and password, we can authenticate with Twitch directly.  This also requires access to the Twitch
			// application's "Client Secret" code.  Because of security issues with manual handling of user names and passwords, this
			// type of authentication to work, your application's Client ID must first be whitelisted by the Twitch developers.
			// @todo twitch: Editor support prompting for username/password if missing? (for direct login support)
			if( !ProjectSettings.UserName.IsEmpty() && !ProjectSettings.Password.IsEmpty() && !ProjectSettings.ClientSecret.IsEmpty() )
			{
				Async_AuthenticateWithTwitchDirectly( ProjectSettings.UserName, ProjectSettings.Password, ProjectSettings.ClientSecret );
			}
			else
			{
				Async_AuthenticateWithTwitchUsingBrowser();
			}
		}
		else if( TwitchState == ETwitchState::WaitingForBrowserBasedAuthentication )
		{
			CheckIfBrowserLoginCompleted();
		}
		else if( TwitchState == ETwitchState::ReadyToLogin )
		{
			Async_LoginToTwitch();
		}
	}

	// Are we logged in yet?
	if( TwitchState == ETwitchState::LoggedIn )
	{
		if( bWantsToBroadcastNow )
		{
			if( BroadcastState == EBroadcastState::Idle )
			{
				Async_GetIngestServers();
			}
			else if( BroadcastState == EBroadcastState::ReadyToBroadcast )
			{
				Async_StartBroadcasting();
			}
		}
		else
		{
			// User does not want to be broadcasting at this time
			if( BroadcastState == EBroadcastState::Broadcasting )
			{
				Async_StopBroadcasting();
			}
		}

		if( bWantsChatEnabled )
		{
			// Startup the chat system if we haven't done that yet
			if( ChatState == EChatState::Uninitialized )
			{
				Async_InitChatSupport();
			}
			else if( ChatState == EChatState::Initialized )
			{
				Async_ConnectToChat();
			}
			else if( ChatState == EChatState::Connected )
			{
				// ...
			}
		}
		else
		{
			if( ChatState == EChatState::Connected )
			{
				Async_DisconnectFromChat();
			}
		}
	}

	if( bWantsWebCamNow )
	{
		// Startup the web cam support if we haven't done that yet
		if( WebCamState == EWebCamState::Uninitialized )
		{
			Async_InitWebCamSupport();
		}
		else if( WebCamState == EWebCamState::Initialized )
		{
			// Do we have a device?
			if( WebCamDeviceIndex != INDEX_NONE )
			{
				Async_StartWebCam();
			}
			else
			{
				// User wants to start a web cam, but we couldn't find one that we support
			}
		}
		else if( WebCamState == EWebCamState::Started )
		{
			UpdateWebCamTexture();
		}
	}
	else
	{
		if( WebCamState == EWebCamState::Started )
		{
			Async_StopWebCam();
		}
	}


	// If we hit a login failure or broadcasting failure, stop waiting to broadcast and wait to be asked again by the caller
	if( BroadcastState == EBroadcastState::BroadcastingFailure )
	{
		UE_LOG( LogTwitch, Warning, TEXT( "After Twitch broadcasting failure, reverting to non-broadcasting state") );
		PublishStatus( EStatusType::BroadcastStopped, LOCTEXT( "Status_BroadcastStoppedUnexpectedly", "Broadcasting has ended unexpectedly." ) );
		BroadcastState = EBroadcastState::Idle;
		bWantsToBroadcastNow = false;
	}
	
	if( TwitchState == ETwitchState::LoginFailure )
	{
		UE_LOG( LogTwitch, Warning, TEXT( "After Twitch authenticate/login failure, reverting to non-broadcasting, non-authenticated state") );
		TwitchState = ETwitchState::ReadyToAuthenticate;
		bWantsToBroadcastNow = false;
		bWantsChatEnabled = false;
	}

	// If the web cam system failed to initialize, we'll just leave it alone
	if( WebCamState == EWebCamState::InitFailure )
	{
		bWantsWebCamNow = false;
	}
	else if( WebCamState == EWebCamState::StartOrStopFailure )
	{
		UE_LOG( LogTwitch, Warning, TEXT( "After Twitch web cam start or stop failure, reverting to web cam disabled state") );
		if( WebCamState == EWebCamState::Started )
		{
			PublishStatus( EStatusType::WebCamStopped, LOCTEXT( "Status_WebCamStoppedUnexpectedly", "Web cam stopped unexpectedly" ) );
		}
		WebCamState = EWebCamState::Initialized;
		bWantsWebCamNow = false;
	}


	// If the chat system failed to initialize, we'll just leave it alone
	if( ChatState == EChatState::InitFailure )
	{
		bWantsChatEnabled = false;
	}
	else if( ChatState == EChatState::ConnectOrDisconnectFailure )
	{
		UE_LOG( LogTwitch, Warning, TEXT( "After Twitch chat connection or disconnection failure, reverting chat to disabled state" ) );
		if( ChatState == EChatState::Connected )
		{
			PublishStatus( EStatusType::ChatDisconnected, LOCTEXT( "Status_ChatStoppedUnexpectedly", "Chat stopped unexpectedly" ) );
		}
		ChatState = EChatState::Initialized;
		bWantsChatEnabled = false;
	}


	// Allow the Twitch SDK to process any callbacks that are pending
	TwitchPollTasks();

	// Process Twitch web cam events and fire off callbacks
	TwitchWebCamFlushEvents();
	
	// Process Twitch chat system events
	TwitchChatFlushEvents();
}


bool FTwitchLiveStreaming::IsTickable() const
{
	// Only tickable when properly initialized
	return ( TwitchState != ETwitchState::Uninitialized && TwitchState != ETwitchState::DLLLoaded );
}


TStatId FTwitchLiveStreaming::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT( FTwitchLiveStreaming, STATGROUP_Tickables );
}


bool FTwitchLiveStreaming::IsTickableWhenPaused() const 
{
	// Always ticked, even when the game is suspended
	return true;
}


bool FTwitchLiveStreaming::IsTickableInEditor() const
{
	// Always ticked, even in the editor
	return true;
}


void FTwitchLiveStreaming::Async_InitWebCamSupport()
{
	check( WebCamState == EWebCamState::Uninitialized );

	void* CallbackUserDataPtr = this;

	// Called when TwitchWebCamInit finishes async work
	TTV_WebCamInitializationCallback TwitchWebCamInitializationCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( ensure( This->WebCamState == EWebCamState::WaitingForInit ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// Web cam system is ready
					UE_LOG( LogTwitch, Display, TEXT( "Twitch web cam support is initialized") );

					This->WebCamState = EWebCamState::Initialized;
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to init Twitch web cam support.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
					This->WebCamState = EWebCamState::InitFailure;
				}
			}
		};

	// Called when a web camera is added or removed
	TTV_WebCamDeviceChangeCallback TwitchWebCamDeviceChangeCallback = []( TTV_WebCamDeviceChange Change, const TTV_WebCamDevice* Device, TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// This callback could come in at pretty much any time
			if( ensure( This->WebCamState != EWebCamState::Uninitialized && This->WebCamState != EWebCamState::WaitingForInit ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// @todo twitch: Warn when no devices are connected at all and users decides they want web cam? (Or have API to determine whether camera is available)
					if( Change == TTV_WEBCAM_DEVICE_FOUND )
					{
						UE_LOG( LogTwitch, Display, TEXT( "Twitch detected a new web cam: %s"), UTF8_TO_TCHAR( Device->name ) );

						// If we don't have a web cam yet, we'll use the first one we find
						// @todo twitch: For now, we're just choosing the first web cam that we find that supports a format we like
						if( This->WebCamDeviceIndex == INDEX_NONE )
						{
							This->WebCamCapabilityIndex = INDEX_NONE;
							// @todo twitch: Not possible to change web cam resolutions after we've inited (even with different params to StartBroadcast), because we never re-enumerate these after initial Init
							const int DesiredPixels = This->WebCamConfig.DesiredWebCamWidth * This->WebCamConfig.DesiredWebCamHeight;
							const float DesiredAspect = (float)This->WebCamConfig.DesiredWebCamWidth / (float)This->WebCamConfig.DesiredWebCamHeight;

							int BestCapabilitySoFar = INDEX_NONE;
							int BestCapabilityPixelDiff = 10000000;
							float BestCapabilityAspectDiff = 100000.0f;
							for( uint32 CapabilityIndex = 0; CapabilityIndex < Device->capabilityList.count; ++CapabilityIndex )
							{
								const auto& Capability = Device->capabilityList.list[ CapabilityIndex ];

								// @todo twitch: Currently we required ARGB32 formats.  We could support others though.
								if( Capability.format == TTV_WEBCAM_FORMAT_ARGB32 )
								{
									const int ThisCapabilityPixels = Capability.resolution.width * Capability.resolution.height;
									const float ThisCapabilityAspect = (float)Capability.resolution.width / (float)Capability.resolution.height;

									const float MaxPixelsVariance = 200 * 200.0f;
									const float MaxAspectVariance = 0.3f;

									const int PixelsDiff = FMath::Abs( ThisCapabilityPixels - DesiredPixels );
									const float AspectDiff = FMath::Abs( ThisCapabilityAspect - DesiredAspect );
									if( BestCapabilitySoFar == INDEX_NONE || 
										
										// Clearly better
										( PixelsDiff < BestCapabilityPixelDiff &&
										  AspectDiff < BestCapabilityAspectDiff ) ||

										// Sort of better.  Closer to desired resolution, but aspect is worse
										( PixelsDiff < BestCapabilityPixelDiff &&
										  AspectDiff < ( BestCapabilityAspectDiff + MaxAspectVariance / 2.0f ) ) ||

										// Sort of better.  Closer to desired aspect, but resolution is worse
										( PixelsDiff < ( BestCapabilityPixelDiff + MaxPixelsVariance / 2 ) &&
										  AspectDiff < BestCapabilityAspectDiff ) )
									{
										// Best one so far
										BestCapabilitySoFar = CapabilityIndex;
										BestCapabilityPixelDiff = PixelsDiff;
										BestCapabilityAspectDiff = AspectDiff;
									}
								}
							}

							if( BestCapabilitySoFar != INDEX_NONE )
							{
								const auto& Capability = Device->capabilityList.list[ BestCapabilitySoFar ];
								
								// OK, we found a format that works
								This->WebCamDeviceIndex = Device->deviceIndex;
								This->WebCamCapabilityIndex = Capability.capabilityIndex;
								This->WebCamVideoBufferWidth = Capability.resolution.width;
								This->WebCamVideoBufferHeight = Capability.resolution.height;
								This->bIsWebCamTextureFlippedVertically = !Capability.isTopToBottom;
							}
							else
							{
								UE_LOG( LogTwitch, Warning, TEXT( "Web cam device '%s' did not have any formats that we could support"), UTF8_TO_TCHAR( Device->name ) );
							}
						}
					}
					else if( Change == TTV_WEBCAM_DEVICE_LOST )
					{
						// Are we already streaming a camera feed?
						if( This->WebCamDeviceIndex != INDEX_NONE )
						{
							// If this was the camera we were streaming with, then we need to unhook everything
							if( (uint32)Device->deviceIndex == This->WebCamDeviceIndex )
							{
								// No longer able to stream this web cam
								This->WebCamDeviceIndex = INDEX_NONE;

								if( This->WebCamState == EWebCamState::Started )
								{
									UE_LOG( LogTwitch, Display, TEXT( "Stopped web cam '%s' because device was unplugged" ), UTF8_TO_TCHAR( Device->name ) );
									This->PublishStatus( EStatusType::WebCamStopped, LOCTEXT( "Status_WebCamUnplugged", "Web cam was unplugged" ) );

									This->WebCamState = EWebCamState::Initialized;

									// Release our texture so it won't be used for rendering anymore
									const bool bReleaseResourceToo = false;	
									This->ReleaseWebCamTexture( bReleaseResourceToo );
								}
							}
						}

						UE_LOG( LogTwitch, Display, TEXT( "Twitch detected a web cam was disconnected: %s"), UTF8_TO_TCHAR( Device->name ) );
					}
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Received an error from Twitch with a web cam device add/remove callback.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );

					// NOTE: We don't bother failing the whole system for an error like this
				}
			}
		};

	UE_LOG( LogTwitch, Display, TEXT( "Initializing Twitch web cam support...") );

	TTV_WebCamCallbacks TwitchWebCamCallbacks;
	TwitchWebCamCallbacks.deviceChangeCallback = TwitchWebCamDeviceChangeCallback;
	TwitchWebCamCallbacks.deviceChangeUserData = CallbackUserDataPtr;

	WebCamState = EWebCamState::WaitingForInit;
	const TTV_ErrorCode TwitchErrorCode = TwitchWebCamInit( &TwitchWebCamCallbacks, TwitchWebCamInitializationCallback, CallbackUserDataPtr );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to start initialization of Twitch web cam support.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		WebCamState = EWebCamState::InitFailure;
	}
}


void FTwitchLiveStreaming::Async_StartWebCam()
{
	check( WebCamState == EWebCamState::Initialized );
	check( WebCamDeviceIndex != INDEX_NONE && WebCamCapabilityIndex != INDEX_NONE );

	void* CallbackUserDataPtr = this;
	
	// @todo twitch: Starting the web cam doesn't seem to automatically start capturing from the default microphone, unless you turn on "What U Hear" in Windows.

	// Called when TwitchWebCamStart finishes async work
	TTV_WebCamDeviceStatusCallback TwitchWebCamStatusCallback = []( const TTV_WebCamDevice* Device, const TTV_WebCamDeviceCapability* Capability, TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( ensure( This->WebCamState == EWebCamState::WaitingForStart ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					UE_LOG( LogTwitch, Display, TEXT( "Started recording using web cam '%s' at %i x %i"), UTF8_TO_TCHAR( Device->name ), Capability->resolution.width, Capability->resolution.height );
					This->PublishStatus( EStatusType::WebCamStarted, LOCTEXT( "Status_WebCamStarted", "Web cam enabled" ) );

					This->WebCamState = EWebCamState::Started;
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to start recording with web cam.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
					This->WebCamState = EWebCamState::StartOrStopFailure;
				}
			}
		};


	UE_LOG( LogTwitch, Display, TEXT( "Starting web cam...") );
	
	WebCamState = EWebCamState::WaitingForStart;
	const TTV_ErrorCode TwitchErrorCode = TwitchWebCamStart( WebCamDeviceIndex, WebCamCapabilityIndex, TwitchWebCamStatusCallback, CallbackUserDataPtr );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to begin starting up web camera.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		WebCamState = EWebCamState::StartOrStopFailure;
	}
}


void FTwitchLiveStreaming::Async_StopWebCam()
{
	check( WebCamState == EWebCamState::Started );

	void* CallbackUserDataPtr = this;

	// Called when TwitchWebCamStop finishes async work
	TTV_WebCamDeviceStatusCallback TwitchWebCamStatusCallback = []( const TTV_WebCamDevice* Device, const TTV_WebCamDeviceCapability* Capability, TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( ensure( This->WebCamState == EWebCamState::WaitingForStop ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					UE_LOG( LogTwitch, Display, TEXT( "Stopped web cam '%s'"), UTF8_TO_TCHAR( Device->name ) );
					This->PublishStatus( EStatusType::WebCamStopped, LOCTEXT( "Status_WebCamStopped", "Web cam is now off" ) );

					This->WebCamState = EWebCamState::Initialized;

					// Release our texture so it won't be used for rendering anymore
					const bool bReleaseResourceToo = false;	
					This->ReleaseWebCamTexture( bReleaseResourceToo );
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to stop recording with web cam.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
					This->WebCamState = EWebCamState::StartOrStopFailure;
				}
			}
		};


	UE_LOG( LogTwitch, Display, TEXT( "Stopping web cam...") );
	
	WebCamState = EWebCamState::WaitingForStop;
	const TTV_ErrorCode TwitchErrorCode = TwitchWebCamStop( WebCamDeviceIndex, TwitchWebCamStatusCallback, CallbackUserDataPtr );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to begin stopping web camera.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		WebCamState = EWebCamState::StartOrStopFailure;
	}
}


void FTwitchLiveStreaming::UpdateWebCamTexture()
{
	check( WebCamState == EWebCamState::Started );

	// Do we have a new texture available?
	bool bIsFrameAvailable = false;
	{
		const TTV_ErrorCode TwitchErrorCode = TwitchWebCamIsFrameAvailable( WebCamDeviceIndex, &bIsFrameAvailable );
		if( !TTV_SUCCEEDED( TwitchErrorCode ) )
		{
			const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );

			UE_LOG( LogTwitch, Verbose, TEXT( "Error while checking to see if new frame from web camera is available.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		}
	}

	bool bWasWebCamTextureCreated = false;
	if( bIsFrameAvailable )
	{
		const uint32 WebCamBufferPitch = WebCamVideoBufferWidth * 4;	// 4 bytes per pixel

		// This will be passed off to the render thread, which will delete it when it has finished with it
		// @todo twitch: This is a bit heavy to keep reallocating/deallocating, but not a big deal.  Maybe we can ping pong between buffers instead.
		uint8* WebCamBuffer = new uint8[ WebCamBufferPitch * WebCamVideoBufferHeight ];

		// Get a copy of the web cam frame data from Twitch
		const TTV_ErrorCode TwitchErrorCode = TwitchWebCamGetFrame( WebCamDeviceIndex, WebCamBuffer, WebCamBufferPitch );
		if( TTV_SUCCEEDED( TwitchErrorCode ) )
		{
			// @todo twitch: If texture format changes, we'd need to re-create it too.  Currently we don't support other formats though.
			if( WebCamTexture == nullptr || WebCamVideoBufferWidth != WebCamTexture->GetSurfaceWidth() || WebCamVideoBufferHeight != WebCamTexture->GetSurfaceHeight() )
			{
				// Texture has not been created yet, or desired size has changed
				const bool bReleaseResourceToo = true;	
				ReleaseWebCamTexture( bReleaseResourceToo );

				// NOTE: We're not really using this texture as a render target, as we map the resource and upload bits to the texture
				// from system memory directly.  We never actually bind it as a render target.  However, this is the best API we have for
				// dealing with textures like this.
				WebCamTexture = UTexture2D::CreateTransient( WebCamVideoBufferWidth, WebCamVideoBufferHeight, PF_B8G8R8A8 );

				// Add the texture to the root set so that it doesn't get garbage collected.  We'll remove it from the root set ourselves when we're done with it.
				WebCamTexture->AddToRoot();

				WebCamTexture->AddressX = TA_Clamp;
				WebCamTexture->AddressY = TA_Clamp;
				WebCamTexture->bIsStreamable = false;
				WebCamTexture->CompressionSettings = TC_Default;
				WebCamTexture->Filter = TF_Bilinear;
				WebCamTexture->LODGroup = TEXTUREGROUP_UI;
				WebCamTexture->NeverStream = true;
				WebCamTexture->SRGB = true;

				WebCamTexture->UpdateResource();

				bWasWebCamTextureCreated = true;
			}

			struct FUploadWebCamTextureContext
			{
				uint8* WebCamBuffer;	// Render thread assumes ownership
				uint32 WebCamBufferPitch;
				FTexture2DResource* DestTextureResource;
			} UploadWebCamTextureContext =
			{
				WebCamBuffer,
				WebCamBufferPitch,
				(FTexture2DResource*)WebCamTexture->Resource
			};

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				UploadWebCamTexture,
				FUploadWebCamTextureContext,Context,UploadWebCamTextureContext,
			{
				const FUpdateTextureRegion2D UpdateRegion(
					0, 0,		// Dest X, Y
					0, 0,		// Source X, Y
					Context.DestTextureResource->GetSizeX(),	// Width
					Context.DestTextureResource->GetSizeY() );	// Height

				RHIUpdateTexture2D(
					Context.DestTextureResource->GetTexture2DRHI(),	// Destination GPU texture
					0,												// Mip map index
					UpdateRegion,									// Update region
					Context.WebCamBufferPitch,						// Source buffer pitch
					Context.WebCamBuffer );							// Source buffer pointer

				// Delete the buffer now that we've uploaded the bytes
				delete[] Context.WebCamBuffer;
			});
		}
		else
		{
			const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
			UE_LOG( LogTwitch, Verbose, TEXT( "Error while requesting web cam frame buffer.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		}
	}

	if( bWasWebCamTextureCreated )
	{
		PublishStatus( EStatusType::WebCamTextureReady, LOCTEXT( "Status_WebCamTextureReady", "Web cam video enabled" ) );
	}
}


void FTwitchLiveStreaming::ReleaseWebCamTexture( const bool bReleaseResourceToo )
{
	if( WebCamTexture != nullptr )
	{
		// Release the web cam texture
		if( bReleaseResourceToo )
		{
			WebCamTexture->ReleaseResource();
		}
		WebCamTexture->RemoveFromRoot();
		WebCamTexture = nullptr;

		bIsWebCamTextureFlippedVertically = false;

		PublishStatus( EStatusType::WebCamTextureNotReady, LOCTEXT( "Status_WebCamTextureNotReady", "Web cam video disabled" ) );
	}
}


void FTwitchLiveStreaming::Async_InitChatSupport()
{
	check( ChatState == EChatState::Uninitialized );

	void* CallbackUserDataPtr = this;

	// Called when TwitchChatInit finishes async work
	TTV_ChatInitializationCallback TwitchChatInitializationCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr ) 
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( ensure( This->ChatState == EChatState::WaitingForInit ) )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// Chat system is ready
					UE_LOG( LogTwitch, Display, TEXT( "Twitch chat support is initialized") );

					This->ChatState = EChatState::Initialized;
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to init Twitch chat support.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
					This->ChatState = EChatState::InitFailure;
				}
			}
		};


	UE_LOG( LogTwitch, Display, TEXT( "Initializing Twitch chat support...") );

	// @todo twitch: Could eventually add support for handling emoticons here, and badges
	const TTV_ChatTokenizationOption TwitchChatTokenizationOption = TTV_CHAT_TOKENIZATION_OPTION_NONE;

	ChatState = EChatState::WaitingForInit;
	const TTV_ErrorCode TwitchErrorCode = TwitchChatInit( TwitchChatTokenizationOption, TwitchChatInitializationCallback, CallbackUserDataPtr );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to start initialization of Twitch chat support.\n\nError: %s (%d)"), *TwitchErrorString, (int32)TwitchErrorCode );
		ChatState = EChatState::InitFailure;
	}
}



void FTwitchLiveStreaming::Async_ConnectToChat()
{
	check( TwitchState == ETwitchState::LoggedIn );
	check( ChatState == EChatState::Initialized );

	void* CallbackUserDataPtr = this;

	// Called when Twitch chat connection succeeds, or when the user becomes disconnected
	TTV_ChatStatusCallback TwitchChatStatusCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr )
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// Make sure we're still in the state that we were expecting
			if( This->ChatState == EChatState::WaitingToConnect )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// Connected to chat!
					UE_LOG( LogTwitch, Display, TEXT( "Connected to chat!" ) );
					This->PublishStatus( EStatusType::ChatConnected, LOCTEXT( "Status_ChatConnected", "Connected to chat server" ) );
					This->ChatState = EChatState::Connected;
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to connect to Twitch chat.\n\nError: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );
					This->ChatState = EChatState::ConnectOrDisconnectFailure;
				}
			}
			else if( This->ChatState == EChatState::WaitingForDisconnect )
			{
				if( TTV_SUCCEEDED( TwitchErrorCode ) )
				{
					// Disconnected from chat!
					UE_LOG( LogTwitch, Display, TEXT( "Disconnected from chat" ) );
					This->PublishStatus( EStatusType::ChatDisconnected, LOCTEXT( "Status_ChatDisconnected", "Disconnected from chat server" ) );
					This->ChatState = EChatState::Initialized;
				}
				else
				{
					const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
					UE_LOG( LogTwitch, Warning, TEXT( "Failed to disconnect from Twitch chat.\n\nError: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );
					This->ChatState = EChatState::ConnectOrDisconnectFailure;
				}
			}
			else
			{
				const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
				UE_LOG( LogTwitch, Warning, TEXT( "Received a chat status message at a time that we were not expected.\n\nError code: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );
			}

		};

	TTV_ChatChannelMembershipCallback TwitchChatChannelMembershipCallback = []( TTV_ChatEvent Event, const TTV_ChatChannelInfo* ChannelInfo, void* CallbackUserDataPtr )
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// @todo twitch: Add chat callbacks for pushing information about the channel back to the game
		};

	TTV_ChatChannelUserChangeCallback TwitchChatChannelUserChangeCallback = []( const TTV_ChatUserList* JoinList, const TTV_ChatUserList* LeaveList, const TTV_ChatUserList* InfoChangeList, void* CallbackUserDataPtr )
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// @todo twitch: Add chat events for users joining/leaving the channel
		};

	TTV_ChatChannelMessageCallback TwitchChatChannelMessageCallback = []( const TTV_ChatRawMessageList* MessageList, void* CallbackUserDataPtr )
		{
			FTwitchLiveStreaming* This = static_cast<FTwitchLiveStreaming*>( CallbackUserDataPtr );

			// We received a chat message.  Let's route it to anyone who is listening!
			if( This->OnChatMessageEvent.IsBound() )
			{
				for( uint32 MessageIndex = 0; MessageIndex < MessageList->messageCount; ++MessageIndex )
				{ 
					const auto& TwitchMessage = MessageList->messageList[ MessageIndex ];

					// @todo twitch: Make use of the other settings in this TTV_ChatRawMessage struct, such as the user name's color and whether
					// the message is an action
					const FText UserNameText( FText::FromString( UTF8_TO_TCHAR( TwitchMessage.userName ) ) );
					const FText MessageText( FText::FromString( UTF8_TO_TCHAR( TwitchMessage.message ) ) );

					This->OnChatMessageEvent.Broadcast( UserNameText, MessageText );
				}
			}
		};

	TTV_ChatChannelTokenizedMessageCallback TwitchChatChannelTokenizedMessageCallback = []( const TTV_ChatTokenizedMessageList* MessageList, void* CallbackUserDataPtr )
		{
			// NOTE: Not currently expecting to ever receive this event, because we connect with the TTV_CHAT_TOKENIZATION_OPTION_NONE option
			check( 0 );
		};

	UE_LOG( LogTwitch, Display, TEXT( "Connecting to chat..." ) );

	const utf8char* TwitchChatUsername = "testing";	// @todo twitch urgent (need GetUserInfo() implemented?  Or allow user to supply chat name)

	// We'll authorize chat using the same token we received after logging into Twitch
	const char* TwitchChatAuthToken = TwitchAuthToken.data;
	check( TwitchChatAuthToken != nullptr );

	TTV_ChatCallbacks TwitchChatCallbacks;
	TwitchChatCallbacks.statusCallback = TwitchChatStatusCallback;	// The callback to call for connection and disconnection events from the chat service. Cannot be NULL.
	TwitchChatCallbacks.membershipCallback = TwitchChatChannelMembershipCallback;	// The callback to call when the local user joins or leaves a channel. Cannot be NULL.
	TwitchChatCallbacks.userCallback = TwitchChatChannelUserChangeCallback;		// The callback to call when other users join or leave a channel. This may be NULL.
	TwitchChatCallbacks.messageCallback = TwitchChatChannelMessageCallback;		// The callback to call when raw messages are received on a channel. 
	TwitchChatCallbacks.tokenizedMessageCallback = TwitchChatChannelTokenizedMessageCallback;		// The callback to call when tokenized messages are received on a channel. 
	TwitchChatCallbacks.clearCallback = nullptr;			// The callback to call when the chatroom should be cleared. Can be NULL.
	TwitchChatCallbacks.unsolicitedUserData = CallbackUserDataPtr;	// The userdata to pass in the callbacks.


	ChatState = EChatState::WaitingToConnect;
	const TTV_ErrorCode TwitchErrorCode = TwitchChatConnect( TwitchChatUsername, TwitchChatAuthToken, &TwitchChatCallbacks );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to begin connecting to chat.\n\nError: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );
		ChatState = EChatState::ConnectOrDisconnectFailure;
	}
}


void FTwitchLiveStreaming::Async_DisconnectFromChat()
{
	check( ChatState == EChatState::Connected );

	UE_LOG( LogTwitch, Display, TEXT( "Disconnecting from chat..." ) );

	ChatState = EChatState::WaitingForDisconnect;
	const TTV_ErrorCode TwitchErrorCode = TwitchChatDisconnect();
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to begin disconnecting from chat.\n\nError: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );
		ChatState = EChatState::ConnectOrDisconnectFailure;
	}
}


void FTwitchLiveStreaming::Async_SendChatMessage( const FString& ChatMessage )
{
	check( ChatState == EChatState::Connected );

	UE_LOG( LogTwitch, Display, TEXT( "Sending chat message: %s" ), *ChatMessage );

	// NOTE: There is no callback when the message is received
	const TTV_ErrorCode TwitchErrorCode = TwitchChatSendMessage( TCHAR_TO_UTF8( *ChatMessage ) );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to send chat message to Twitch.\n\nError: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );
	}
}


void FTwitchLiveStreaming::Async_GetGameLiveStreams( const FString& GameName, FQueryLiveStreamsCallback CompletionCallback )
{
	struct FCallbackPayload
	{
		FTwitchLiveStreaming* This;

		/** Name of the game we're querying streams for */
		FString GameName;

		/** List of game streams queried from TwitchGetGameLiveStreams */
		TTV_LiveGameStreamList TwitchGameStreamList;

		/** Callback to run when we're done */
		FQueryLiveStreamsCallback CompletionCallback;
	};

	auto* CallbackPayload = new FCallbackPayload();	// This will be deleted by the async callback function below
	CallbackPayload->This = this;
	CallbackPayload->GameName = GameName;
	FMemory::Memzero( CallbackPayload->TwitchGameStreamList );
	CallbackPayload->CompletionCallback = CompletionCallback;

	void* CallbackUserDataPtr = CallbackPayload;

	// Called when TwitchGetGameLiveStreams finishes async work
	TTV_TaskCallback TwitchTaskCallback = []( TTV_ErrorCode TwitchErrorCode, void* CallbackUserDataPtr )
	{
		FCallbackPayload* CallbackPayload = static_cast<FCallbackPayload*>( CallbackUserDataPtr );
		FTwitchLiveStreaming* This = CallbackPayload->This;

		bool bQueryWasSuccessful = true;
		if( TTV_SUCCEEDED( TwitchErrorCode ) )
		{
			UE_LOG( LogTwitch, Display, TEXT( "Successfully queried live game stream list" ) );
		}
		else
		{
			bQueryWasSuccessful = false;
			const FString TwitchErrorString( UTF8_TO_TCHAR( This->TwitchErrorToString( TwitchErrorCode ) ) );
			UE_LOG( LogTwitch, Warning, TEXT( "Failed to query game live streams.\n\nError: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );
		}

		// Build up an Unreal-friendly list of streams
		TArray< FLiveStreamInfo > ListOfStreams;
		for( uint32 InfoIndex = 0; InfoIndex < CallbackPayload->TwitchGameStreamList.count; ++InfoIndex )
		{
			const auto& TwitchLiveGameStreamInfo = CallbackPayload->TwitchGameStreamList.list[ InfoIndex ];

			FLiveStreamInfo StreamInfo;
			StreamInfo.GameName = CallbackPayload->GameName;
			StreamInfo.StreamName = TwitchLiveGameStreamInfo.streamTitle;	// @todo twitch: Also include TwitchLiveGameStreamInfo.channelDisplayName?
			StreamInfo.URL = TwitchLiveGameStreamInfo.channelUrl;
			ListOfStreams.Add( StreamInfo );
		}

		// Free the list of game streams that Twitch allocated
		if( CallbackPayload->TwitchGameStreamList.list != nullptr )
		{
			This->TwitchFreeGameLiveStreamList( &CallbackPayload->TwitchGameStreamList );
		}

		// Run the user's callback!
		CallbackPayload->CompletionCallback.ExecuteIfBound( ListOfStreams, bQueryWasSuccessful );

		// Delete the callback payload now that we're done with it
		delete CallbackPayload;
	};


	UE_LOG( LogTwitch, Display, TEXT( "Querying game live streams for '%s'..." ), *GameName );

	const TTV_ErrorCode TwitchErrorCode = TwitchGetGameLiveStreams( TCHAR_TO_ANSI( *GameName ), TwitchTaskCallback, CallbackUserDataPtr, &CallbackPayload->TwitchGameStreamList );
	if( !TTV_SUCCEEDED( TwitchErrorCode ) )
	{
		const FString TwitchErrorString( UTF8_TO_TCHAR( TwitchErrorToString( TwitchErrorCode ) ) );
		UE_LOG( LogTwitch, Warning, TEXT( "Failed to start querying game live streams.\n\nError: %s (%d)" ), *TwitchErrorString, (int32)TwitchErrorCode );

		// Run the user's callback, but report a failure message
		const TArray< FLiveStreamInfo > EmptyListOfStreams;
		const bool bQueryWasSuccessful = false;
		CompletionCallback.ExecuteIfBound( EmptyListOfStreams, bQueryWasSuccessful );
	}
}


#undef LOCTEXT_NAMESPACE

#else	// WITH_TWITCH

class FTwitchLiveStreaming_NoTwitchSDK : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		UE_LOG( LogTwitch, Verbose, TEXT( "TwitchLiveStreaming plugin was built without Twitch SDK.  Twitch features will not be available." ) );
	}
};

IMPLEMENT_MODULE( FTwitchLiveStreaming_NoTwitchSDK, TwitchLiveStreaming )

#endif
