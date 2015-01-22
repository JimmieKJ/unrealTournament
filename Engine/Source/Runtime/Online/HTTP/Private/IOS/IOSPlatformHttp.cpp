// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "IOSPlatformHttp.h"
#include "IOSHttp.h"

void FIOSPlatformHttp::Init()
{
}


void FIOSPlatformHttp::Shutdown()
{
}


IHttpRequest* FIOSPlatformHttp::ConstructRequest()
{
	return new FIOSHttpRequest();
}
