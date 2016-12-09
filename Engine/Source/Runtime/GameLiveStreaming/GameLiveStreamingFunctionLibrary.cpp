// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "GameLiveStreamingFunctionLibrary.h"
#include "IGameLiveStreaming.h"

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
		const FString& LoginUserName,
		const FString& LoginPassword,
		int32 FrameRate,
		float ScreenScaling,
		bool bStartWebCam,
		int32 DesiredWebCamWidth,
		int32 DesiredWebCamHeight,
		bool bMirrorWebCamImage,
		bool bDrawSimpleWebCamVideo,
		bool bCaptureAudioFromComputer,
		bool bCaptureAudioFromMicrophone,
		UTexture2D* CoverUpImage )
{
	FGameBroadcastConfig Config;
	Config.LoginUserName = LoginUserName;
	Config.LoginPassword = LoginPassword;
	Config.FrameRate = FrameRate;
	Config.ScreenScaling = ScreenScaling;
	Config.bStartWebCam = bStartWebCam;
	Config.WebCamConfig.DesiredWebCamWidth = DesiredWebCamWidth;
	Config.WebCamConfig.DesiredWebCamHeight = DesiredWebCamHeight;
	Config.WebCamConfig.bMirrorWebCamImage = bMirrorWebCamImage;
	Config.WebCamConfig.bDrawSimpleWebCamVideo = bDrawSimpleWebCamVideo;
	Config.bCaptureAudioFromComputer = bCaptureAudioFromComputer;
	Config.bCaptureAudioFromMicrophone = bCaptureAudioFromMicrophone;
	Config.CoverUpImage = CoverUpImage;
	IGameLiveStreaming::Get().StartBroadcastingGame( Config );
}


void UGameLiveStreamingFunctionLibrary::StopBroadcastingGame()
{
	IGameLiveStreaming::Get().StopBroadcastingGame();
}


bool UGameLiveStreamingFunctionLibrary::IsWebCamEnabled()
{
	return IGameLiveStreaming::Get().IsWebCamEnabled();
}


void UGameLiveStreamingFunctionLibrary::StartWebCam(
		int32 DesiredWebCamWidth,
		int32 DesiredWebCamHeight,
		bool bMirrorWebCamImage,
		bool bDrawSimpleWebCamVideo )
{
	FGameWebCamConfig Config;
	Config.DesiredWebCamWidth = DesiredWebCamWidth;
	Config.DesiredWebCamHeight = DesiredWebCamHeight;
	Config.bMirrorWebCamImage = bMirrorWebCamImage;
	Config.bDrawSimpleWebCamVideo = bDrawSimpleWebCamVideo;
	IGameLiveStreaming::Get().StartWebCam( Config );
}


void UGameLiveStreamingFunctionLibrary::StopWebCam()
{
	IGameLiveStreaming::Get().StopWebCam();
}


#undef LOCTEXT_NAMESPACE
