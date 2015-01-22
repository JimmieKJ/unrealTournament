// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "HTML5PlatformHttp.h"
#include "HTML5Http.h"


void FHTML5PlatformHttp::Init()
{
}


void FHTML5PlatformHttp::Shutdown()
{
}


IHttpRequest* FHTML5PlatformHttp::ConstructRequest()
{
	return new FHTML5HttpRequest();
}