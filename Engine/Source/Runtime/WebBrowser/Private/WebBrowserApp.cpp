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
    std::string a = "d"+x;
}


/*void FWebBrowserApp::OnContextCreated(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefV8Context> Context)
{
    // Retrieve the context's window object.
    CefRefPtr<CefV8Value> Object = Context->GetGlobal();
    
    Object->SetValue("answer",
                     CefV8Value::CreateUInt(42),
                     V8_PROPERTY_ATTRIBUTE_READONLY);
}*/

#endif
