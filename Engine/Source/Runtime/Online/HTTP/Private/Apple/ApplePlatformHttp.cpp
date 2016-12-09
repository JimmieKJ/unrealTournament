// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
