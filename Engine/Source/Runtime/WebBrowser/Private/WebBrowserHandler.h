// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#include "include/cef_client.h"
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

// Forward Declarations
class FWebBrowserWindow;

/**
 * Implements CEF Client and other Browser level interfaces 
 */
class FWebBrowserHandler : public CefClient,
	public CefDisplayHandler,
	public CefLifeSpanHandler,
	public CefLoadHandler,
	public CefRenderHandler,
	public CefRequestHandler
{
public:
	/**
	 * Default Constructor
	 */
	FWebBrowserHandler();

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

	// CefDisplayHandler Interface
	virtual void OnTitleChange(CefRefPtr<CefBrowser> Browser, const CefString& Title) override;

	// CefLifeSpanHandler Interface
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> Browser) override;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> Browser) override;

	// CefLoadHandler Interface
	virtual void OnLoadError(CefRefPtr<CefBrowser> Browser,
		CefRefPtr<CefFrame> Frame,
		CefLoadHandler::ErrorCode InErrorCode,
		const CefString& ErrorText,
		const CefString& FailedUrl) override;

	// CefRenderHandler Interface
	virtual bool GetViewRect(CefRefPtr<CefBrowser> Browser, CefRect& Rect) override;
	virtual void OnPaint(CefRefPtr<CefBrowser> Browser,
		PaintElementType Type,
		const RectList& DirtyRects,
		const void* Buffer,
		int Width, int Height) override;
	virtual void OnCursorChange(CefRefPtr<CefBrowser> Browser, CefCursorHandle Cursor) override;

	// CefRequestHandler Interface
	virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> Browser,
		CefRefPtr<CefFrame> Frame,
		CefRefPtr<CefRequest> Request) override;

	/**
	 * Pass in a pointer to our Browser Window so that events can be passed on
	 *
	 * @param InBrowserWindow The browser window this will be handling
	 */
	void SetBrowserWindow(TSharedPtr<FWebBrowserWindow> InBrowserWindow);

private:
	/** Weak Pointer to our Web Browser window so that events can be passed on while it's valid*/
	TWeakPtr<FWebBrowserWindow> BrowserWindow;

	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(FWebBrowserHandler);
};
#endif
