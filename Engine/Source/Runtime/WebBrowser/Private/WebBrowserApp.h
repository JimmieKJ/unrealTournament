// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
#include "include/cef_app.h"
#pragma pop_macro("OVERRIDE")
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

/**
 * Implements CEF App and other Process level interfaces 
 */
class FWebBrowserApp : public CefApp,
	public CefBrowserProcessHandler
{
public:
	
	/**
	 * Default Constructor
	 */
	FWebBrowserApp();

	/** A delegate this is invoked when an existing browser requests creation of a new browser window. */
	DECLARE_DELEGATE_OneParam(FOnRenderProcessThreadCreated, CefRefPtr<CefListValue> /*ExtraInfo*/);
	virtual FOnRenderProcessThreadCreated& OnRenderProcessThreadCreated()
	{
		return RenderProcessThreadCreatedDelegate;
	}

private:
	// CefApp methods.
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

	// CefBrowserProcessHandler methods:
	virtual void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine) override;
	virtual void OnRenderProcessThreadCreated(CefRefPtr<CefListValue> ExtraInfo) override;

	FOnRenderProcessThreadCreated RenderProcessThreadCreatedDelegate;

	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(FWebBrowserApp);
};
#endif
