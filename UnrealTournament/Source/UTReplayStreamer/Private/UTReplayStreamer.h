// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HttpNetworkReplayStreaming.h"
#include "OnlineSubsystem.h"
#include "OnlineIdentityInterface.h"

class FUTReplayStreamer : public FHttpNetworkReplayStreamer
{
public:
	FUTReplayStreamer();

	virtual void ProcessRequestInternal( TSharedPtr< class IHttpRequest > Request ) override;

	IOnlineIdentityPtr OnlineIdentityInterface;
};

class FUTReplayStreamingFactory : public FHttpNetworkReplayStreamingFactory
{
public:
	/** INetworkReplayStreamingFactory */
	virtual TSharedPtr< INetworkReplayStreamer > CreateReplayStreamer() override;
};
