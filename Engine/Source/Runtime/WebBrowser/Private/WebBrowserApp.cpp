// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "WebBrowserApp.h"

#if WITH_CEF3
FWebBrowserApp::FWebBrowserApp()
{
}

void FWebBrowserApp::OnContextInitialized()
{
	// Called on the browser process UI thread immediately after the CEF context
 	// has been initialized.
}

void FWebBrowserApp::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine)
{
	std::string x = CommandLine->GetCommandLineString();
	std::string a = "d" + x;
}

bool FWebBrowserApp::OnBeforeNavigation(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefRequest> Request, NavigationType Navigation_Type, bool Is_redirect)
{
	return false;
}


#endif
