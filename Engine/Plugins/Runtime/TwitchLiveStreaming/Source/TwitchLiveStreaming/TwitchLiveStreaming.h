// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "ITwitchLiveStreaming.h"
#include "Runtime/Engine/Public/Features/ILiveStreamingService.h"
#include "Runtime/Engine/Public/Tickable.h"

#if WITH_TWITCH

// Twitch SDK includes
#include "twitchsdk.h"
#include "twitchwebcam.h"
#include "twitchchat.h"


/**
 * Twitch live streaming implementation
 */
class FTwitchLiveStreaming : public ITwitchLiveStreaming, public ILiveStreamingService, public FTickableGameObject
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** ILiveStreamingService implementation */
	virtual FOnStatusChanged& OnStatusChanged() override;
	virtual void StartBroadcasting( const FBroadcastConfig& Config ) override;
	virtual void StopBroadcasting() override;
	virtual bool IsBroadcasting() const override;
	virtual bool IsReadyForVideoFrames() const override;
	virtual void MakeValidVideoBufferResolution( int& VideoBufferWidth, int& VideoBufferHeight ) const override;
	virtual void QueryBroadcastConfig( FBroadcastConfig& OutBroadcastConfig ) const override;
	virtual void PushVideoFrame( const FColor* VideoFrameBuffer ) override;
	virtual void StartWebCam( const FWebCamConfig& Config ) override;
	virtual void StopWebCam() override;
	virtual bool IsWebCamEnabled() const override;
	virtual UTexture2D* GetWebCamTexture( bool& bIsImageFlippedHorizontally, bool& bIsImageFlippedVertically ) override;
	virtual FOnChatMessage& OnChatMessage() override;
	virtual void ConnectToChat() override;
	virtual void DisconnectFromChat() override;
	virtual bool IsChatEnabled() const override;
	virtual void SendChatMessage( const FString& ChatMessage ) override;
	virtual void QueryLiveStreams( const FString& GameName, FQueryLiveStreamsCallback CompletionCallback ) override;

private:

	/** Initializes Twitch lazily */
	void InitOnDemand();

	/** Loads the Twitch SDK dynamic library */
	void LoadTwitchSDK( const FString& TwitchDLLFolder );

	struct FTwitchProjectSettings
	{
		FString ClientID;
		FString RedirectURI;
		FString UserName;
		FString Password;
		FString ClientSecret;
	};

	/** Loads settings for Twitch from project configuration files */
	FTwitchProjectSettings LoadProjectSettings();

	/** Initializes the Twitch SDK */
	void InitTwitch( const FString& TwitchWindowsDLLFolder );

	/** Tries to authenticate credentials with Twitch by connecting directly to the service */
	void Async_AuthenticateWithTwitchDirectly( const FString& UserName, const FString& Password, const FString& ClientSecret );

	/** Tries to authenticate credentials with Twitch using a web browser to login */
	void Async_AuthenticateWithTwitchUsingBrowser();

	/** Checks to see if the user has successfully authenticated with Twitch using the web browser */
	void CheckIfBrowserLoginCompleted();

	/** Attempts to login to the Twitch server */
	void Async_LoginToTwitch();

	/** Queries available servers */
	void Async_GetIngestServers();

	/** Starts broadcasting video and audio */
	void Async_StartBroadcasting();

	/** Stops broadcasting */
	void Async_StopBroadcasting();

	/** Transmits a single frame of video to Twitch */
	void Async_SendVideoFrame( const FColor* VideoFrameBuffer );

	typedef FLiveStreamingStatus::EStatusType EStatusType;

	/** Executes user callbacks to let the user know about some kind of Twitch status change */
	void PublishStatus( const EStatusType Type, const FText& Text );

	/** FTickableGameObject implementation */
	virtual void Tick( float DeltaTime ) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override;
	virtual bool IsTickableInEditor() const override;


private:

	/** The various states we can be in.  Nearly everything happens asynchronously, so we track what's going on with a simple state machine. */
	enum class ETwitchState
	{
		Uninitialized,
		DLLLoaded,
		ReadyToAuthenticate,
		WaitingForBrowserBasedAuthentication,
		WaitingForDirectAuthentication,
		ReadyToLogin,
		WaitingForLogin,
		LoggedIn,
		LoginFailure,
	};

	/** We'll broadcast this event when any status changes happen to Twitch, such as logging in or activating a web camera */
	FOnStatusChanged OnStatusChangedEvent;

	/** Current state that we're in */
	ETwitchState TwitchState;

	/** Handle to the Twitch DLL */
	void* TwitchDLLHandle;

	/** Settings for Twitch, loaded from project configuration files */
	FTwitchProjectSettings ProjectSettings;

	/** Twitch authorization token, valid after authorization completes and kept around so we can pass it to various Twitch SDK functions */
	TTV_AuthToken TwitchAuthToken;

	/** Time that we started browser-based login.  This is used to timeout a request that is unresponsive. */
	FDateTime BrowserLoginStartTime;

	/** Time that we last checked to see if browser-based login had completed.  This is used to wait between retries, to reduce CPU usage. */
	FDateTime LastBrowserLoginCheckTime;


	/**
	 * Twitch init and authentication functions
	 */

	typedef TTV_ErrorCode( *TwitchSetTraceLevelFuncPtr )( TTV_MessageLevel traceLevel );
	TwitchSetTraceLevelFuncPtr TwitchSetTraceLevel;

	typedef TTV_ErrorCode( *TwitchInitFuncPtr )( const TTV_MemCallbacks* memCallbacks, const char* clientID, const wchar_t* dllPath );
	TwitchInitFuncPtr TwitchInit;

	typedef TTV_ErrorCode( *TwitchShutdownFuncPtr )( );
	TwitchShutdownFuncPtr TwitchShutdown;

	typedef TTV_ErrorCode( *TwitchPollTasksFuncPtr )( );
	TwitchPollTasksFuncPtr TwitchPollTasks;

	typedef TTV_ErrorCode( *TwitchRequestAuthTokenFuncPtr )( const TTV_AuthParams* authParams, uint32_t flags, TTV_TaskCallback callback, void* userData, TTV_AuthToken* authToken );
	TwitchRequestAuthTokenFuncPtr TwitchRequestAuthToken;

	typedef TTV_ErrorCode( *TwitchLoginFuncPtr )( const TTV_AuthToken* authToken, TTV_TaskCallback callback, void* userData, TTV_ChannelInfo* channelInfo );
	TwitchLoginFuncPtr TwitchLogin;

	typedef const char* ( *TwitchErrorToStringFuncPtr )( TTV_ErrorCode err );
	TwitchErrorToStringFuncPtr TwitchErrorToString;


	/**
	 * Live broadcasting support
	 */

	/** Broadcasting states */
	enum class EBroadcastState
	{
		Idle,
		WaitingForGetIngestServers,
		ReadyToBroadcast,
		WaitingToStartBroadcasting,
		Broadcasting,
		WaitingToStopBroadcasting,
		BroadcastingFailure,
	};

	/** Current broadcasting state */
	EBroadcastState BroadcastState;

	/** True if the user wants to be broadcasting right now */
	bool bWantsToBroadcastNow;

	/** User's configuration of this broadcast */
	FBroadcastConfig BroadcastConfig;

	/** Info about the current channel (only valid after login completes) */
	TTV_ChannelInfo TwitchChannelInfo;

	/** List of ingest servers available to broadcast using. */
	TTV_IngestList TwitchIngestList;

	/** Number of video buffers that Twitch requires */
	static const uint32 TwitchVideoBufferCount = 3;

	/** List of three capture buffers that Twitch requires in order to stream video.  We'll cycle through them every time
	    a new video frame is pushed to Twitch */
	uint8* VideoBuffers[ TwitchVideoBufferCount ];

	/** List of buffers that are currently available.  This array contains a subset of buffers in the VideoBuffers list that are
	    currently available for streaming. */
	TArray< uint8* > AvailableVideoBuffers;


	/**
	 * Twitch broadcasting functions
	 */

	typedef TTV_ErrorCode( *TwitchGetIngestServersFuncPtr )( const TTV_AuthToken* authToken, TTV_TaskCallback callback, void* userData, TTV_IngestList* ingestList );
	TwitchGetIngestServersFuncPtr TwitchGetIngestServers;

	typedef TTV_ErrorCode( *TwitchFreeIngestListFuncPtr )( TTV_IngestList* ingestList );
	TwitchFreeIngestListFuncPtr TwitchFreeIngestList;

	typedef TTV_ErrorCode( *TwitchStartFuncPtr )( const TTV_VideoParams* videoParams, const TTV_AudioParams* audioParams, const TTV_IngestServer* ingestServer, uint32_t flags, TTV_TaskCallback callback, void* userData );
	TwitchStartFuncPtr TwitchStart;

	typedef TTV_ErrorCode( *TwitchStopFuncPtr )( TTV_TaskCallback callback, void* userData );
	TwitchStopFuncPtr TwitchStop;

	typedef TTV_ErrorCode( *TwitchGetMaxResolutionFuncPtr )( uint maxKbps, uint frameRate, float bitsPerPixel, float aspectRatio, uint* width, uint* height );
	TwitchGetMaxResolutionFuncPtr TwitchGetMaxResolution;

	typedef TTV_ErrorCode( *TwitchGetDefaultParamsFuncPtr )( TTV_VideoParams* videoParams );
	TwitchGetDefaultParamsFuncPtr TwitchGetDefaultParams;

	typedef TTV_ErrorCode( *TwitchSubmitVideoFrameFuncPtr )( const uint8_t* frameBuffer, TTV_BufferUnlockCallback callback, void* userData );
	TwitchSubmitVideoFrameFuncPtr TwitchSubmitVideoFrame;



	/**
	 * WebCam support
	 */

	/** Updates the web cam texture by copying the latest frame from the camera to a GPU texture asynchronously */
	void UpdateWebCamTexture();

	/** Starts initializing web cam support */
	void Async_InitWebCamSupport();

	/** Starts recording from the web camera */
	void Async_StartWebCam();

	/** Stops recording from the web camera */
	void Async_StopWebCam();

	/** Releases the web cam texture */
	void ReleaseWebCamTexture( const bool bReleaseResourceToo );


	/** The various web cam states we can be in */
	enum class EWebCamState
	{
		Uninitialized,
		WaitingForInit,
		Initialized,
		InitFailure,
		WaitingForStart,
		Started,
		WaitingForStop,
		StartOrStopFailure
	};

	/** True if the user wants to show a web cam right now */
	bool bWantsWebCamNow;

	/** User's web cam configuration */
	FWebCamConfig WebCamConfig;

	/** Current web cam state that we're in */
	EWebCamState WebCamState;

	/** Selected web cam device index */
	int32 WebCamDeviceIndex;

	/** Selected web cam capability index (basically the video resolution) */
	int32 WebCamCapabilityIndex;

	/** Web cam video buffer width */
	int32 WebCamVideoBufferWidth;

	/** Web cam video buffer height */
	int32 WebCamVideoBufferHeight;

	/** True if the web cam texture is flipped vertically, as captured from the camera, and needs to be displayed bottom to top */
	bool bIsWebCamTextureFlippedVertically;

	/** Texture that stores the web cam image, copied asynchronously from the CPU */
	class UTexture2D* WebCamTexture;


	/**
	 * Twitch WebCam SDK functions
	 */

	typedef TTV_ErrorCode ( *TwitchWebCamInitFuncPtr )( const TTV_WebCamCallbacks* interruptCallbacks, TTV_WebCamInitializationCallback initCallback, void* userdata );
	TwitchWebCamInitFuncPtr TwitchWebCamInit;

	typedef TTV_ErrorCode ( *TwitchWebCamShutdownFuncPtr )( TTV_WebCamShutdownCallback callback, void* userdata );
	TwitchWebCamShutdownFuncPtr TwitchWebCamShutdown;

	typedef TTV_ErrorCode ( *TwitchWebCamStartFuncPtr )( int deviceIndex, int capabilityIndex, TTV_WebCamDeviceStatusCallback callback, void* userdata );
	TwitchWebCamStartFuncPtr TwitchWebCamStart;

	typedef TTV_ErrorCode ( *TwitchWebCamStopFuncPtr )( int deviceIndex, TTV_WebCamDeviceStatusCallback callback, void* userdata );
	TwitchWebCamStopFuncPtr TwitchWebCamStop;

	typedef TTV_ErrorCode ( *TwitchWebCamFlushEventsFuncPtr )();
	TwitchWebCamFlushEventsFuncPtr TwitchWebCamFlushEvents;

	typedef TTV_ErrorCode ( *TwitchWebCamIsFrameAvailableFuncPtr )( int deviceIndex, bool* available );
	TwitchWebCamIsFrameAvailableFuncPtr TwitchWebCamIsFrameAvailable;

	typedef TTV_ErrorCode ( *TwitchWebCamGetFrameFuncPtr )( int deviceIndex, void* buffer, unsigned int pitch );
	TwitchWebCamGetFrameFuncPtr TwitchWebCamGetFrame;



	/**
	 * Chat support
	 */

	/** Initializes Twitch's chat system asynchronously */
	void Async_InitChatSupport();

	/** Tries to connect to Twitch's chat server */
	void Async_ConnectToChat();

	/** Quits the chat connection */
	void Async_DisconnectFromChat();

	/** Sends a chat message.  You need to be connected for this to work! */
	void Async_SendChatMessage( const FString& ChatMessage );

	/** The various chat states we can be in */
	enum class EChatState
	{
		Uninitialized,
		WaitingForInit,
		Initialized,
		InitFailure,
		WaitingToConnect,
		Connected,
		WaitingForDisconnect,
		ConnectOrDisconnectFailure
	};

	/** True if the user wants to connect to chat right now */
	bool bWantsChatEnabled;

	/** Current chat state that we're in */
	EChatState ChatState;

	/** Broadcast whenever we receive a chat message from the server */
	FOnChatMessage OnChatMessageEvent;


	/**
	 * Twitch Chat SDK functions
	 */

	typedef TTV_ErrorCode( *TwitchChatInitFuncPtr )( TTV_ChatTokenizationOption tokenizationOptions, TTV_ChatInitializationCallback callback, void* userdata );
	TwitchChatInitFuncPtr TwitchChatInit;

	typedef TTV_ErrorCode( *TwitchChatShutdownFuncPtr )( TTV_ChatShutdownCallback callback, void* userdata );
	TwitchChatShutdownFuncPtr TwitchChatShutdown;

	typedef TTV_ErrorCode( *TwitchChatConnectFuncPtr )( const utf8char* username, const char* authToken, const TTV_ChatCallbacks* chatCallbacks );
	TwitchChatConnectFuncPtr TwitchChatConnect;

	typedef TTV_ErrorCode( *TwitchChatDisconnectFuncPtr )();
	TwitchChatDisconnectFuncPtr TwitchChatDisconnect;

	typedef TTV_ErrorCode( *TwitchChatSendMessageFuncPtr )( const utf8char* message );
	TwitchChatSendMessageFuncPtr TwitchChatSendMessage;

	typedef TTV_ErrorCode( *TwitchChatFlushEventsFuncPtr )();
	TwitchChatFlushEventsFuncPtr TwitchChatFlushEvents;



	/**
	 * Query operations
	 */

	/** Kicks off a query for a list of game live streams */
	void Async_GetGameLiveStreams( const FString& GameName, FQueryLiveStreamsCallback CompletionCallback );


	/**
	 * Twitch miscellaneous SDK functions
	 */
	 
	typedef TTV_ErrorCode( *TwitchGetGameLiveStreamsFuncPtr )( const char* gameName, TTV_TaskCallback callback, void* userData, TTV_LiveGameStreamList* gameStreamList );
	TwitchGetGameLiveStreamsFuncPtr TwitchGetGameLiveStreams;

	typedef TTV_ErrorCode( *TwitchFreeGameLiveStreamListFuncPtr )( TTV_LiveGameStreamList* gameStreamList );
	TwitchFreeGameLiveStreamListFuncPtr TwitchFreeGameLiveStreamList;

};


#endif	// WITH_TWITCH