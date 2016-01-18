// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3

#if PLATFORM_WINDOWS
	#include "AllowWindowsPlatformTypes.h"
#endif

#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
#include "include/cef_client.h"
#pragma pop_macro("OVERRIDE")

#if PLATFORM_WINDOWS
	#include "HideWindowsPlatformTypes.h"
#endif


class FWebBrowserWindow;
class FWebBrowserPopupFeatures;


/**
 * Implements CEF Client and other Browser level interfaces.
 */
class FWebBrowserHandler
	: public CefClient
	, public CefDisplayHandler
	, public CefLifeSpanHandler
	, public CefLoadHandler
	, public CefRenderHandler
	, public CefRequestHandler
	, public CefKeyboardHandler
	, public CefJSDialogHandler
{
public:

	/** Default constructor. */
	FWebBrowserHandler();

public:

	/**
	 * Pass in a pointer to our Browser Window so that events can be passed on.
	 *
	 * @param InBrowserWindow The browser window this will be handling.
	 */
	void SetBrowserWindow(TSharedPtr<FWebBrowserWindow> InBrowserWindow);
	
	/**
	 * Pass in a pointer to our parent Browser Window.
	 *
	 * @param InBrowserWindow The parent browser window.
	 */
	void SetBrowserWindowParent(TSharedPtr<FWebBrowserWindow> InBrowserWindow);

	/**
	 * Sets the browser window features and settings for popups which will be passed along when creating the new window.
	 *
	 * @param InPopupFeatures The popup features and settings for the window.
	 */
	void SetPopupFeatures(const TSharedPtr<FWebBrowserPopupFeatures>& InPopupFeatures)
	{
		BrowserPopupFeatures = InPopupFeatures;
	}

public:

	// CefClient Interface

	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override
	{
		return this;
	}

	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override
	{
		return this;
	}

	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override
	{
		return this;
	}

	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override
	{
		return this;
	}

	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override
	{
		return this;
	}

	virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override
	{
		return this;
	}

	virtual CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override
	{
		return this;
	}

	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> Browser,
		CefProcessId SourceProcess,
		CefRefPtr<CefProcessMessage> Message) override;


public:

	// CefDisplayHandler Interface

	virtual void OnTitleChange(CefRefPtr<CefBrowser> Browser, const CefString& Title) override;
	virtual void OnAddressChange(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame, const CefString& Url) override;
	virtual bool OnTooltip(CefRefPtr<CefBrowser> Browser, CefString& Text) override;

public:

	// CefLifeSpanHandler Interface

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> Browser) override;
	virtual bool DoClose(CefRefPtr<CefBrowser> Browser) override;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> Browser) override;

	virtual bool OnBeforePopup(CefRefPtr<CefBrowser> Browser,
		CefRefPtr<CefFrame> Frame,
		const CefString& Target_Url,
		const CefString& Target_Frame_Name,
		CefLifeSpanHandler::WindowOpenDisposition /* Target_Disposition */,
		bool /* User_Gesture */,
		const CefPopupFeatures& PopupFeatures,
		CefWindowInfo& WindowInfo,
		CefRefPtr<CefClient>& Client,
		CefBrowserSettings& Settings,
		bool* no_javascript_access) override 
	{
		return OnBeforePopup(Browser, Frame, Target_Url, Target_Frame_Name, PopupFeatures, WindowInfo, Client, Settings, no_javascript_access);
	}

	virtual bool OnBeforePopup(CefRefPtr<CefBrowser> Browser,
		CefRefPtr<CefFrame> Frame, 
		const CefString& Target_Url, 
		const CefString& Target_Frame_Name,
		const CefPopupFeatures& PopupFeatures, 
		CefWindowInfo& WindowInfo,
		CefRefPtr<CefClient>& Client, 
		CefBrowserSettings& Settings,
		bool* no_javascript_access) ;

public:

	// CefLoadHandler Interface

	virtual void OnLoadError(CefRefPtr<CefBrowser> Browser,
		CefRefPtr<CefFrame> Frame,
		CefLoadHandler::ErrorCode InErrorCode,
		const CefString& ErrorText,
		const CefString& FailedUrl) override;

	virtual void OnLoadingStateChange(
		CefRefPtr<CefBrowser> browser,
		bool isLoading,
		bool canGoBack,
		bool canGoForward) override;

	virtual void OnLoadStart(CefRefPtr<CefBrowser> Browser, CefRefPtr<CefFrame> Frame) override;


public:

	// CefRenderHandler Interface
	virtual bool GetRootScreenRect(CefRefPtr<CefBrowser> Browser, CefRect& Rect) override;
	virtual bool GetViewRect(CefRefPtr<CefBrowser> Browser, CefRect& Rect) override;
	virtual void OnPaint(CefRefPtr<CefBrowser> Browser,
		PaintElementType Type,
		const RectList& DirtyRects,
		const void* Buffer,
		int Width, int Height) override;
	virtual void OnCursorChange(CefRefPtr<CefBrowser> Browser,
		CefCursorHandle Cursor,
		CefRenderHandler::CursorType Type,
		const CefCursorInfo& CustomCursorInfo) override;
	virtual void OnPopupShow(CefRefPtr<CefBrowser> Browser, bool bShow) override;
	virtual void OnPopupSize(CefRefPtr<CefBrowser> Browser, const CefRect& Rect) override;

public:

	// CefRequestHandler Interface

	virtual ReturnValue OnBeforeResourceLoad(
		CefRefPtr<CefBrowser> Browser,
		CefRefPtr<CefFrame> Frame,
		CefRefPtr<CefRequest> Request,
		CefRefPtr<CefRequestCallback> /* Callback */) override
	{
		return OnBeforeResourceLoad(Browser, Frame, Request) ? RV_CANCEL : RV_CONTINUE;
	}

	virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> Browser,
		CefRefPtr<CefFrame> Frame,
		CefRefPtr<CefRequest> Request);

	virtual void OnRenderProcessTerminated(CefRefPtr<CefBrowser> Browser, TerminationStatus Status) override;
	virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> Browser,
		CefRefPtr<CefFrame> Frame,
		CefRefPtr<CefRequest> Request,
		bool IsRedirect) override;
	virtual CefRefPtr<CefResourceHandler> GetResourceHandler( CefRefPtr<CefBrowser> Browser, CefRefPtr< CefFrame > Frame, CefRefPtr<CefRequest> Request ) override;

public:
	// CefKeyboardHandler interface
	virtual bool OnKeyEvent(CefRefPtr<CefBrowser> Browser,
		const CefKeyEvent& Event,
		CefEventHandle OsEvent) override;

public:
	// CefJSDialogHandler interface

	virtual bool OnJSDialog(CefRefPtr<CefBrowser> Browser, const CefString& OriginUrl, const CefString& AcceptLang, JSDialogType DialogType, const CefString& MessageText, const CefString& DefaultPromptText, CefRefPtr<CefJSDialogCallback> Callback, bool& OutSuppressMessage) override;

	virtual bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> Browser, const CefString& MessageText, bool IsReload, CefRefPtr<CefJSDialogCallback> Callback) override;

	virtual void OnResetDialogState(CefRefPtr<CefBrowser> Browser) override;

private:

	bool ShowDevTools(const CefRefPtr<CefBrowser>& Browser);

	/** Weak Pointer to our Web Browser window so that events can be passed on while it's valid*/
	TWeakPtr<FWebBrowserWindow> BrowserWindowPtr;
	
	/** Weak Pointer to the parent web browser window */
	TWeakPtr<FWebBrowserWindow> BrowserWindowParentPtr;

	/** Stores popup window features and settings */
	TSharedPtr<FWebBrowserPopupFeatures> BrowserPopupFeatures;
	
	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(FWebBrowserHandler);
};

#endif
