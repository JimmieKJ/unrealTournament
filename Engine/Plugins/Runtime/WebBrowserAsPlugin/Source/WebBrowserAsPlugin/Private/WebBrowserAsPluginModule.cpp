// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserAsPluginPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// FWebBrowserAsPluginModule

class FWebBrowserAsPluginModule : public IWebBrowserAsPluginModule
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

IMPLEMENT_MODULE(FWebBrowserAsPluginModule, WebBrowserAsPlugin);
DEFINE_LOG_CATEGORY(LogWebBrowserAsPlugin);
