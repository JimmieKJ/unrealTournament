//
//  UnrealCEFSubProcessApp.cpp
//  UE4
//
//  Created by Hrafnkell Freyr Hlodversson on 21/03/15.
//  Copyright (c) 2015 EpicGames. All rights reserved.
//

#include "UnrealCEFSubProcess.h"
#include "UnrealCEFSubProcessApp.h"

#if WITH_CEF3

FUnrealCEFSubProcessApp::FUnrealCEFSubProcessApp()
{
    CefMessageRouterConfig MessageRouterConfig;
    MessageRouterConfig.js_query_function = "ueQuery";
    MessageRouterConfig.js_cancel_function = "ueQueryCancel";
    MessageRouter = CefMessageRouterRendererSide::Create(MessageRouterConfig);
}


void FUnrealCEFSubProcessApp::OnContextCreated(CefRefPtr<CefBrowser> Browser,
    CefRefPtr<CefFrame> Frame,
    CefRefPtr<CefV8Context> Context)
{
    MessageRouter->OnContextCreated(Browser, Frame, Context);
}

void FUnrealCEFSubProcessApp::OnContextReleased(CefRefPtr<CefBrowser> Browser,
    CefRefPtr<CefFrame> Frame,
    CefRefPtr<CefV8Context> Context)
{
    MessageRouter->OnContextReleased(Browser, Frame, Context);
}

bool FUnrealCEFSubProcessApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> Browser,
    CefProcessId SourceProcess,
    CefRefPtr<CefProcessMessage> Message)
{
    return MessageRouter->OnProcessMessageReceived(Browser, SourceProcess, Message);
}

#endif