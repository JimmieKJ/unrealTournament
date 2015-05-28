// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserWidgetPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// FWebBrowserWidgetModule

class FWebBrowserWidgetModule : public IWebBrowserWidgetModule
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FWebBrowserWidgetModule, WebBrowserWidget);
DEFINE_LOG_CATEGORY(LogWebBrowserWidget);
