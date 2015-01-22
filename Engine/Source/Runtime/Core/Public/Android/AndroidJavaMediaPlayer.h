// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "AndroidJava.h"

// Wrapper for com/epicgames/ue4/MediaPlayer*.java.
class FJavaAndroidMediaPlayer : public FJavaClassObject
{
public:
	FJavaAndroidMediaPlayer(bool swizzlePixels = true);
	int32 GetDuration();
	void Reset();
	void Stop();
	int32 GetCurrentPosition();
	bool IsLooping();
	bool IsPlaying();
	void SetDataSource(const FString & Url);
	bool SetDataSource(const FString& MoviePathOnDevice, int64 offset, int64 size);
	bool SetDataSource(jobject AssetMgr, const FString& AssetPath, int64 offset, int64 size);
	void Prepare();
	void SeekTo(int32 Milliseconds);
	void SetLooping(bool Looping);
	void Release();
	int32 GetVideoHeight();
	int32 GetVideoWidth();
	void SetVideoEnabled(bool enabled = true);
	void SetAudioEnabled(bool enabled = true);
	bool GetVideoLastFrameData(void* & outPixels, int64 & outCount);
	void Start();
	void Pause();
	bool GetVideoLastFrame(int32 destTexture);

private:
	static FName GetClassName();

	FJavaClassMethod GetDurationMethod;
	FJavaClassMethod ResetMethod;
	FJavaClassMethod StopMethod;
	FJavaClassMethod GetCurrentPositionMethod;
	FJavaClassMethod IsLoopingMethod;
	FJavaClassMethod IsPlayingMethod;
	FJavaClassMethod SetDataSourceURLMethod;
	FJavaClassMethod SetDataSourceFileMethod;
	FJavaClassMethod SetDataSourceAssetMethod;
	FJavaClassMethod PrepareMethod;
	FJavaClassMethod SeekToMethod;
	FJavaClassMethod SetLoopingMethod;
	FJavaClassMethod ReleaseMethod;
	FJavaClassMethod GetVideoHeightMethod;
	FJavaClassMethod GetVideoWidthMethod;
	FJavaClassMethod SetVideoEnabledMethod;
	FJavaClassMethod SetAudioEnabledMethod;
	FJavaClassMethod GetVideoLastFrameDataMethod;
	FJavaClassMethod StartMethod;
	FJavaClassMethod PauseMethod;
	FJavaClassMethod GetVideoLastFrameMethod;
};
