// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "ApplePlatformHttp.h"
#include "AppleHTTP.h"


void FApplePlatformHttp::Init()
{
}


void FApplePlatformHttp::Shutdown()
{
}


IHttpRequest* FApplePlatformHttp::ConstructRequest()
{
	return new FAppleHttpRequest();
}
