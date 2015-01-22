// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "GameLiveStreamingModule.h"
#include "GameLiveStreamingFunctionLibrary.h"
#include "Public/IGameLiveStreaming.h"

#define LOCTEXT_NAMESPACE "GameLiveStreaming"


UGameLiveStreamingFunctionLibrary::UGameLiveStreamingFunctionLibrary( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}


bool UGameLiveStreamingFunctionLibrary::IsBroadcastingGame()
{
	return IGameLiveStreaming::Get().IsBroadcastingGame();
}


void UGameLiveStreamingFunctionLibrary::StartBroadcastingGame(
		int32 FrameRate,
		float ScreenScaling,
		bool bEnableWebCam,
		int32 DesiredWebCamWidth,
		int32 DesiredWebCamHeight,
		bool bMirrorWebCamImage,
		bool bCaptureAudioFromComputer,
		bool bCaptureAudioFromMicrophone,
		bool bDrawSimpleWebCamVideo )
{
	FGameBroadcastConfig Config;
	Config.FrameRate = FrameRate;
	Config.ScreenScaling = ScreenScaling;
	Config.bEnableWebCam = bEnableWebCam;
	Config.DesiredWebCamWidth = DesiredWebCamWidth;
	Config.DesiredWebCamHeight = DesiredWebCamHeight;
	Config.bMirrorWebCamImage = bMirrorWebCamImage;
	Config.bCaptureAudioFromComputer = bCaptureAudioFromComputer;
	Config.bCaptureAudioFromMicrophone = bCaptureAudioFromMicrophone;
	Config.bDrawSimpleWebCamVideo = bDrawSimpleWebCamVideo;
	IGameLiveStreaming::Get().StartBroadcastingGame( Config );
}


void UGameLiveStreamingFunctionLibrary::StopBroadcastingGame()
{
	IGameLiveStreaming::Get().StopBroadcastingGame();
}



#undef LOCTEXT_NAMESPACE
