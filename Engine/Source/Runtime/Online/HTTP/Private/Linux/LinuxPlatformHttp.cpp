// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

void FLinuxPlatformHttp::Init()
{
	FCurlHttpManager::InitCurl();
}

class FHttpManager * FLinuxPlatformHttp::CreatePlatformHttpManager()
{
	return new FCurlHttpManager();
}

void FLinuxPlatformHttp::Shutdown()
{
	FCurlHttpManager::ShutdownCurl();
}

IHttpRequest* FLinuxPlatformHttp::ConstructRequest()
{
	return new FCurlHttpRequest(FCurlHttpManager::GMultiHandle);
}

