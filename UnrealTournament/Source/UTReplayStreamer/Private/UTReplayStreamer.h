// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HttpNetworkReplayStreaming.h"

class FUTReplayStreamer : public FHttpNetworkReplayStreamer
{
public:
	FUTReplayStreamer();

};

class FUTReplayStreamingFactory : public FHttpNetworkReplayStreamingFactory
{
public:
	/** INetworkReplayStreamingFactory */
	virtual TSharedPtr< INetworkReplayStreamer > CreateReplayStreamer() override;
};
