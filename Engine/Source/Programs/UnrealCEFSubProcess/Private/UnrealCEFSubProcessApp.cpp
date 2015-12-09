// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

void FUnrealCEFSubProcessApp::OnContextCreated( CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefV8Context> Context )
{
	MessageRouter->OnContextCreated(Browser, Frame, Context);
	RemoteScripting.OnContextCreated(Browser, Frame, Context);
}

void FUnrealCEFSubProcessApp::OnContextReleased( CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefV8Context> Context )
{
	MessageRouter->OnContextReleased(Browser, Frame, Context);
	RemoteScripting.OnContextReleased(Browser, Frame, Context);
}

bool FUnrealCEFSubProcessApp::OnProcessMessageReceived( CefRefPtr<CefBrowser> Browser, CefProcessId SourceProcess, CefRefPtr<CefProcessMessage> Message )
{
	bool Result = false;
	FString MessageName = Message->GetName().ToWString().c_str();
	if (MessageName.StartsWith(TEXT("UE::")))
	{
		Result = RemoteScripting.OnProcessMessageReceived(Browser, SourceProcess, Message);
	}
	else
	{
		Result = MessageRouter->OnProcessMessageReceived(Browser, SourceProcess, Message);
	}

	return Result;
}

void FUnrealCEFSubProcessApp::OnRenderThreadCreated( CefRefPtr<CefListValue> ExtraInfo )
{
	for(size_t I=0; I<ExtraInfo->GetSize(); I++)
	{
		if (ExtraInfo->GetType(I) == VTYPE_DICTIONARY)
		{
			CefRefPtr<CefDictionaryValue> Info = ExtraInfo->GetDictionary(I);
			if ( Info->GetType("browser") == VTYPE_INT)
			{
				int32 BrowserID = Info->GetInt("browser");
				CefRefPtr<CefDictionaryValue> Bindings = Info->GetDictionary("bindings");
				RemoteScripting.InitPermanentBindings(BrowserID, Bindings);
			}
		}
	}
}

#endif