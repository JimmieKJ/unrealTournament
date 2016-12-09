// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "HttpNetworkReplayStreaming.h"
#include "OnlineSubsystem.h"
#include "OnlineIdentityInterface.h"

class FUTReplayStreamer : public FHttpNetworkReplayStreamer
{
public:
	FUTReplayStreamer();

	virtual void ProcessRequestInternal( TSharedPtr< class IHttpRequest > Request ) override;
};

class FUTReplayStreamingFactory : public FHttpNetworkReplayStreamingFactory
{
public:
	/** INetworkReplayStreamingFactory */
	virtual TSharedPtr< INetworkReplayStreamer > CreateReplayStreamer() override;
};
