// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameLiveStreamingFunctionLibrary.generated.h"

UCLASS()
class UGameLiveStreamingFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()


	/**
	 * Checks to see if we are currently broadcasting live video (and possibly audio) from the game's viewport
	 *
	 * @return	True if we are currently transmitting
	 */
	UFUNCTION( BlueprintPure, Category="LiveStreaming" )
	static bool IsBroadcastingGame();

	/**
	 * Starts broadcasting the game's video (and optionally audio) using an internet streaming service, if one is available 
	 *
	 * @param	FrameRate					Frame rate to stream video from when broadcasting to services like Twitch.
	 * @param	ScreenScaling				How much to scale the broadcast video resolution down to reduce streaming bandwidth.  We recommend broadcasting at resolutions of 1280x720 or lower.  Some live streaming providers will not be able to transcode your video to a lower resolution, so using a high resolution stream may prevent low-bandwidth users from having a good viewing experience.
	 * @param	bEnableWebCam				If enabled, video from your web camera will be captured and displayed while broadcasting, so that your viewers can see your presence.
	 * @param	DesiredWebCamWidth			Desired web cam capture resolution width.  The web cam may only support a limited number of resolutions, so we'll choose one that matches as closely to this as possible
	 * @param	DesiredWebCamHeight			Desired web cam capture resolution height.
	 * @param	bMirrorWebCamImage			You can enable this to flip the web camera image horizontally, so that it looks like a mirror
	 * @param	bCaptureAudioFromComputer	Enables broadcast of audio being played by your computer, such as in-game sounds
	 * @param	bCaptureAudioFromMicrophone	Enables broadcast of audio from your default microphone recording device
	 * @param	bDrawSimpleWebCamVideo		If enabled, the engine will draw a simple web cam image on top of the game viewport.  If you turn this off, it's up to you to draw the web cam image yourself.  You can access the web cam texture by calling IGameLiveStreaming::Get().GetWebCamTexture(). 
	 */
	UFUNCTION( BlueprintCallable, Category="LiveStreaming")
	static void StartBroadcastingGame(
		int32 FrameRate = 30,
		float ScreenScaling = 1.f,
		bool bEnableWebCam = true,
		int32 DesiredWebCamWidth = 320,
		int32 DesiredWebCamHeight = 240,
		bool bMirrorWebCamImage = false,
		bool bCaptureAudioFromComputer = true,
		bool bCaptureAudioFromMicrophone = true,
		bool bDrawSimpleWebCamVideo = true);

	/** Stops broadcasting the game */
	UFUNCTION( BlueprintCallable, Category="LiveStreaming" )
	static void StopBroadcastingGame();

	// @todo livestream: Add Blueprint APIs for chat
};
