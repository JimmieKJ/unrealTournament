// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Runtime/NetworkReplayStreaming/NetworkReplayStreaming/Public/NetworkReplayStreaming.h"

#if !UE_SERVER

struct FReplayEntry
{
	FNetworkReplayStreamInfo StreamInfo;
	FString		Date;
	FString		Size;
	int32		ResultsIndex;
};

class UNREALTOURNAMENT_API SUWReplayBrowser : public SUWPanel
{
public:

private:

	virtual void ConstructPanel(FVector2D ViewportSize);

protected:

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;

	TSharedPtr<INetworkReplayStreamer> ReplayStreamer;
	TArray< TSharedPtr<FReplayEntry> > ReplayList;
	TSharedPtr<class SButton>  WatchReplayButton;

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);

	bool bShouldShowAllReplays;

	void BuildReplayList();
	void OnEnumerateStreamsComplete(const TArray<FNetworkReplayStreamInfo>& Streams);

	virtual FReply OnWatchClick();
};

#endif