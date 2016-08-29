// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "WebBrowserApp.h"

#if WITH_CEF3
FWebBrowserApp::FWebBrowserApp()
{
}

void FWebBrowserApp::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine)
{
}

void FWebBrowserApp::OnBeforeCommandLineProcessing(const CefString& ProcessType, CefRefPtr< CefCommandLine > CommandLine)
{
	CommandLine->AppendSwitch("disable-gpu");
	CommandLine->AppendSwitch("disable-gpu-compositing");
	CommandLine->AppendSwitch("enable-begin-frame-scheduling");
}

void FWebBrowserApp::OnRenderProcessThreadCreated(CefRefPtr<CefListValue> ExtraInfo)
{
	RenderProcessThreadCreatedDelegate.ExecuteIfBound(ExtraInfo);
}

#endif
