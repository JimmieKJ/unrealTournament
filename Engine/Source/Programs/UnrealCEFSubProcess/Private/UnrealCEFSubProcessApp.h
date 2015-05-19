// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3
#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

/**
 * Implements CEF App and other Process level interfaces
 */
class FUnrealCEFSubProcessApp
    : public CefApp
    , public CefRenderProcessHandler
{
public:
    
    /**
     * Default Constructor
     */
    FUnrealCEFSubProcessApp();
private:
    // CefApp methods.
    virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    // CefRenderProcessHandler methods:
    virtual void OnContextCreated(CefRefPtr<CefBrowser> Browser,
        CefRefPtr<CefFrame> Frame,
        CefRefPtr<CefV8Context> Context) override;

    virtual void OnContextReleased(CefRefPtr<CefBrowser> Browser,
        CefRefPtr<CefFrame> Frame,
        CefRefPtr<CefV8Context> Context) override;

    virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> Browser,
        CefProcessId SourceProcess,
        CefRefPtr<CefProcessMessage> Message) override;

    // Routes messages from Javascript to the browser process and back
    CefRefPtr<CefMessageRouterRendererSide> MessageRouter;

    // Include the default reference counting implementation.
    IMPLEMENT_REFCOUNTING(FUnrealCEFSubProcessApp);
};

#endif // WITH_CEF3