// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWebBrowserSingleton.h"

#if WITH_CEF3
#if PLATFORM_WINDOWS
	#include "AllowWindowsPlatformTypes.h"
#endif
#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
#include "include/internal/cef_ptr.h"
#include "include/cef_request_context.h"
#pragma pop_macro("OVERRIDE")
#if PLATFORM_WINDOWS
	#include "HideWindowsPlatformTypes.h"
#endif

class CefListValue;
#endif


class FWebBrowserApp;
class FWebBrowserHandler;
class FWebBrowserWindow;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/**
 * Implementation of singleton class that takes care of general web browser tasks
 */
class FWebBrowserSingleton
	: public IWebBrowserSingleton
	, public FTickerObjectBase
{
public:

	/** Default constructor. */
	FWebBrowserSingleton();

	/** Virtual destructor. */
	virtual ~FWebBrowserSingleton();

	/**
	* Gets the Current Locale Code in the format CEF expects
	*
	* @return Locale code as either "xx" or "xx-YY"
	*/
	static FString GetCurrentLocaleCode();

public:

	// IWebBrowserSingleton Interface

	virtual TSharedRef<IWebBrowserWindowFactory> GetWebBrowserWindowFactory() const override;

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		TSharedPtr<FWebBrowserWindow>& BrowserWindowParent,
		TSharedPtr<FWebBrowserWindowInfo>& BrowserWindowInfo) override;

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		void* OSWindowHandle, 
		FString InitialURL, 
		bool bUseTransparency,
		bool bThumbMouseButtonNavigation,
		TOptional<FString> ContentsToLoad = TOptional<FString>(), 
		bool ShowErrorMessage = true,
		FColor BackgroundColor = FColor(255, 255, 255, 255),
		int BrowserFrameRate = 24 ) override;

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(const FCreateBrowserWindowSettings& Settings) override;

	virtual void DeleteBrowserCookies(FString URL = TEXT(""), FString CookieName = TEXT(""), TFunction<void(int)> Completed = nullptr) override;

	virtual TSharedPtr<IWebBrowserCookieManager> GetCookieManager() const override
	{
		return DefaultCookieManager;
	}

	virtual TSharedPtr<IWebBrowserCookieManager> GetCookieManager(TOptional<FString> ContextId) const override;

	virtual bool RegisterContext(const FBrowserContextSettings& Settings) override;

	virtual bool UnregisterContext(const FString& ContextId) override;

	virtual bool IsDevToolsShortcutEnabled() override
	{
		return bDevToolsShortcutEnabled;
	}

	virtual void SetDevToolsShortcutEnabled(bool Value) override
	{
		bDevToolsShortcutEnabled = Value;
	}

public:

	// FTickerObjectBase Interface

	virtual bool Tick(float DeltaTime) override;

private:

	TSharedPtr<IWebBrowserCookieManager> DefaultCookieManager;

#if WITH_CEF3
	/** When new render processes are created, send all permanent variable bindings to them. */
	void HandleRenderProcessCreated(CefRefPtr<CefListValue> ExtraInfo);
	/** Pointer to the CEF App implementation */
	CefRefPtr<FWebBrowserApp>			WebBrowserApp;
	/** List of currently existing browser windows */
	TArray<TWeakPtr<FWebBrowserWindow>>	WindowInterfaces;

	TMap<FString, CefRefPtr<CefRequestContext>> RequestContexts;
#endif

	TSharedRef<IWebBrowserWindowFactory> WebBrowserWindowFactory;

	bool bDevToolsShortcutEnabled;

};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_CEF3

class CefCookieManager;

class FCefWebBrowserCookieManagerFactory
{
public:
	static TSharedRef<IWebBrowserCookieManager> Create(
		const CefRefPtr<CefCookieManager>& CookieManager);
};

#endif