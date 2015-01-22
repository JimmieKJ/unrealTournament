// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ModuleManager.h"


/**
 * Settings for a live game broadcast
 */
struct FGameBroadcastConfig
{
	/** Frame rate to stream video when broadcasting to services like Twitch. */
	int32 FrameRate;

	/** How much to scale the broadcast video resolution down to reduce streaming bandwidth.  We recommend broadcasting at resolutions of 1280x720 or lower.  Some live streaming providers will not be able to transcode your video to a lower resolution, so using a high resolution stream may prevent low-bandwidth users from having a good viewing experience.*/
	float ScreenScaling;

	/** If enabled, video from your web camera will be captured and displayed while broadcasting, so that your viewers can see your presence. */
	bool bEnableWebCam;

	/** Desired web cam capture resolution width.  The web cam may only support a limited number of resolutions, so we'll
		choose one that matches as closely to this as possible */
	int32 DesiredWebCamWidth;

	/** Desired web cam capture resolution height. */
	int32 DesiredWebCamHeight;

	/** You can enable this to flip the web camera image horizontally, so that it looks like a mirror */
	bool bMirrorWebCamImage;

	/** Enables broadcast of audio being played by your computer, such as in-game sounds */
	bool bCaptureAudioFromComputer;

	/** Enables broadcast of audio from your default microphone recording device */
	bool bCaptureAudioFromMicrophone;

	/** If enabled, the engine will draw a simple web cam image on top of the game viewport.  If you turn this off, it's up to you to draw the web cam image yourself.  You can access the web cam texture by calling IGameLiveStreaming::Get().GetWebCamTexture(). */
	bool bDrawSimpleWebCamVideo;


	/**
	 * Default constructor
	 */
	FGameBroadcastConfig()
		: FrameRate( 30 ),
		  ScreenScaling( 1.0f ),
		  bEnableWebCam( true ),
		  DesiredWebCamWidth( 320 ),
		  DesiredWebCamHeight( 240 ),
		  bMirrorWebCamImage( false ),
		  bCaptureAudioFromComputer( true ),
		  bCaptureAudioFromMicrophone( true ),
		  bDrawSimpleWebCamVideo( true )
	{
	}
};


/**
 * Interface to the GameLiveStreaming module's functionality for broadcasting live gameplay to the internet
 */
class IGameLiveStreaming
	: public IModuleInterface
{

public:

	/**
	 * Checks to see if we are currently broadcasting live video (and possibly audio) from the game's viewport
	 *
	 * @return	True if we are currently transmitting
	 */
	virtual bool IsBroadcastingGame() const = 0;

	/**
	 * Starts broadcasting the game's video (and optionally audio) using an internet streaming service, if one is available 
	 *
	 * @param	Config	Settings for this game broadcast
	 */
	virtual void StartBroadcastingGame( const FGameBroadcastConfig& GameBroadcastConfig ) = 0;

	/** Stops broadcasting the game */
	virtual void StopBroadcastingGame() = 0;

	/** 
	 * Call this in your game's UI rendering code to draw a simple default web cam image while broadcasting.  Instead of
	 * using this, you can grab the web cam texture yourself by calling GetWebCamTexture() and draw it however you want!
	 *
	 * @param	Canvas	The canvas to draw the web cam image to
	 */
	virtual void DrawSimpleWebCamVideo( UCanvas* Canvas ) = 0;

	/**
	 * Exposes access to the web cam video feed as a 2D texture
	 *
	 * @param	bIsImageFlippedHorizontally		[Out] True if the image is flipped left to right, and you should compensate while drawing by mirroring the texture U coordinates
	 * @param	bIsImageFlippedVertically		[Out] True if the image is flipped top to bottom, and you should compensate while drawing by mirroring the texture V coordinates
	 * 
	 * @return	The web cam video feed texture, or null if the web cam isn't enabled
	 */
	virtual class UTexture2D* GetWebCamTexture( bool& bIsImageFlippedHorizontally, bool& bIsImageFlippedVertically ) = 0;

	/**
	 * Gets direct access to the live streaming service we are using, or nullptr if we haven't selected one yet
	 *
	 * @return	Live streaming service interface
	 */
	virtual class ILiveStreamingService* GetLiveStreamingService() = 0;


public:

	/**
	 * Singleton-like access to IGameLiveStreaming
	 *
	 * @return Returns instance of IGameLiveStreaming object
	 */
	static inline IGameLiveStreaming& Get()
	{
		return FModuleManager::LoadModuleChecked< IGameLiveStreaming >( "GameLiveStreaming" );
	}

};


