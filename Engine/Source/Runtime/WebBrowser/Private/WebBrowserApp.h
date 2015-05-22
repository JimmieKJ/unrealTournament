// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#include "include/cef_app.h"
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

/**
 * Implements CEF App and other Process level interfaces 
 */
class FWebBrowserApp : public CefApp
	,public CefBrowserProcessHandler
	,public CefRenderProcessHandler
{
public:
	
	/**
	 * Default Constructor
	 */
	FWebBrowserApp();
private:
	// CefApp methods.
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

	virtual bool OnBeforeNavigation(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefRequest> Request, NavigationType Navigation_Type, bool Is_redirect) override;

	// CefBrowserProcessHandler methods:
	virtual void OnContextInitialized() override;
	virtual void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine) override;

	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(FWebBrowserApp);
};
#endif
