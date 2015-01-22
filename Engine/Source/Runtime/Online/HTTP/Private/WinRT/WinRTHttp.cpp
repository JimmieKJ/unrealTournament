// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "WinRTHttp.h"


void FWinRTPlatformHttp::Init()
{
}


void FWinRTPlatformHttp::Shutdown()
{
}


IHttpRequest* FWinRTPlatformHttp::ConstructRequest()
{
	return FGenericPlatformHttp::ConstructRequest();
}