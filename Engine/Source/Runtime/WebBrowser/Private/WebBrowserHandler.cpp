// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "WebBrowserHandler.h"
#include "WebBrowserWindow.h"
#include "WebBrowserSingleton.h"

#define LOCTEXT_NAMESPACE "WebBrowserHandler"

#if WITH_CEF3
FWebBrowserHandler::FWebBrowserHandler()
	: ShowErrorMessage(true)
{
    // This has to match the config in UnrealCEFSubpProcess
    CefMessageRouterConfig MessageRouterConfig;
    MessageRouterConfig.js_query_function = "ueQuery";
    MessageRouterConfig.js_cancel_function = "ueQueryCancel";
    MessageRouter = CefMessageRouterBrowserSide::Create(MessageRouterConfig);
}

void FWebBrowserHandler::OnTitleChange(CefRefPtr<CefBrowser> Browser, const CefString& Title)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->SetTitle(Title);
	}
}

void FWebBrowserHandler::OnAddressChange(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, const CefString& Url)
{
	if (Frame->IsMain())
	{
		TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

		if (BrowserWindow.IsValid())
		{
			BrowserWindow->SetUrl(Url);
		}
	}
}

void FWebBrowserHandler::OnAfterCreated(CefRefPtr<CefBrowser> Browser)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->BindCefBrowser(Browser);
	}
}

void FWebBrowserHandler::OnBeforeClose(CefRefPtr<CefBrowser> Browser)
{
    MessageRouter->OnBeforeClose(Browser);
	TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->BindCefBrowser(nullptr);
	}
}

void FWebBrowserHandler::OnLoadError(CefRefPtr<CefBrowser> Browser,
	CefRefPtr<CefFrame> Frame,
	CefLoadHandler::ErrorCode InErrorCode,
	const CefString& ErrorText,
	const CefString& FailedUrl)
{
	// Don't display an error for downloaded files.
	if (InErrorCode == ERR_ABORTED)
	{
		return;
	}

	// notify browser window
	if (Frame->IsMain())
	{
		TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

		if (BrowserWindow.IsValid())
		{
			BrowserWindow->NotifyDocumentError();
		}
	}

	// Display a load error message.
	if (ShowErrorMessage)
	{
		FFormatNamedArguments Args;
		{
			Args.Add(TEXT("FailedUrl"), FText::FromString(FailedUrl.ToWString().c_str()));
			Args.Add(TEXT("ErrorText"), FText::FromString(ErrorText.ToWString().c_str()));
			Args.Add(TEXT("ErrorCode"), FText::AsNumber(InErrorCode));
		}
		FText ErrorMsg = FText::Format(LOCTEXT("WebBrowserLoadError", "Failed to load URL {FailedUrl} with error {ErrorText} ({ErrorCode})."), Args);
		FString ErrorHTML = TEXT("<html><body bgcolor=\"white\"><h2>") + ErrorMsg.ToString() + TEXT("</h2></body></html>");

		Frame->LoadString(*ErrorHTML, FailedUrl);
	}
}

void FWebBrowserHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->NotifyDocumentLoadingStateChange(isLoading);
	}
}

bool FWebBrowserHandler::GetViewRect(CefRefPtr<CefBrowser> Browser, CefRect& Rect)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		return BrowserWindow->GetViewRect(Rect);
	}

	return false;
}

void FWebBrowserHandler::OnPaint(CefRefPtr<CefBrowser> Browser,
	PaintElementType Type,
	const RectList& DirtyRects,
	const void* Buffer,
	int Width, int Height)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnPaint(Type, DirtyRects, Buffer, Width, Height);
	}
}

void FWebBrowserHandler::OnCursorChange(CefRefPtr<CefBrowser> Browser, CefCursorHandle Cursor)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindow = BrowserWindowPtr.Pin();

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnCursorChange(Cursor);
	}
}

bool FWebBrowserHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, CefRefPtr<CefRequest> Request)
{
	const FString LanguageHeaderText(TEXT("Accept-Language"));
	const FString LocaleCode = FWebBrowserSingleton::GetCurrentLocaleCode();
	CefRequest::HeaderMap HeaderMap;
	Request->GetHeaderMap(HeaderMap);
	auto LanguageHeader = HeaderMap.find(*LanguageHeaderText);
	if (LanguageHeader != HeaderMap.end())
	{
		(*LanguageHeader).second = *LocaleCode;
	}
	else
	{
		HeaderMap.insert(std::pair<CefString, CefString>(*LanguageHeaderText, *LocaleCode));
	}
	Request->SetHeaderMap(HeaderMap);
	return false;
}

void FWebBrowserHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> Browser, TerminationStatus Status)
{
    MessageRouter->OnRenderProcessTerminated(Browser);
}

bool FWebBrowserHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> Browser,
    CefRefPtr<CefFrame> Frame,
    CefRefPtr<CefRequest> Request,
    bool IsRedirect)
{
    MessageRouter->OnBeforeBrowse(Browser, Frame);
    return false;
}

void FWebBrowserHandler::SetBrowserWindow(TSharedPtr<FWebBrowserWindow> InBrowserWindow)
{
	BrowserWindowPtr = InBrowserWindow;
}

bool FWebBrowserHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> Browser,
    CefProcessId SourceProcess,
    CefRefPtr<CefProcessMessage> Message)
{
    return MessageRouter->OnProcessMessageReceived(Browser, SourceProcess, Message);
}

bool FWebBrowserHandler::OnQuery(CefRefPtr<CefBrowser> Browser,
	CefRefPtr<CefFrame> Frame,
	int64 QueryId,
	const CefString& Request,
	bool Persistent,
	CefRefPtr<CefMessageRouterBrowserSide::Callback> Callback)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindowPin = BrowserWindowPtr.Pin();
	if (BrowserWindowPin.IsValid())
	{
		return BrowserWindowPin->OnQuery(QueryId, Request, Persistent, Callback);
	}
	return false;
}

void FWebBrowserHandler::OnQueryCanceled(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, int64 QueryId)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindowPin = BrowserWindowPtr.Pin();
	if (BrowserWindowPin.IsValid())
	{
		BrowserWindowPin->OnQueryCanceled(QueryId);
	}
}

bool FWebBrowserHandler::OnBeforePopup(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, const CefString& Target_Url, const CefString& Target_Frame_Name,
	const CefPopupFeatures& PopupFeatures, CefWindowInfo& WindowInfo, CefRefPtr<CefClient>& Client, CefBrowserSettings& Settings,
	bool* no_javascript_access)
{
	TSharedPtr<FWebBrowserWindow> BrowserWindowPin = BrowserWindowPtr.Pin();
	if (BrowserWindowPin.IsValid())
	{
		return BrowserWindowPin->OnCefBeforePopup(Target_Url, Target_Frame_Name);
	}

	return false;
}

#endif

#undef LOCTEXT_NAMESPACE
