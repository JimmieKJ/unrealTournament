// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWebBrowserSingleton.h"

#if WITH_CEF3
#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#include "include/internal/cef_ptr.h"
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif
#endif

// Forward Declarations
class FWebBrowserApp;
class FWebBrowserHandler;
class FWebBrowserWindow;

/**
 * Implementation of singleton class that takes care of general web browser tasks
 */
class FWebBrowserSingleton : public IWebBrowserSingleton, public FTickerObjectBase
{
public:
	/**
	 * Default Constructor
	 */
	FWebBrowserSingleton();
	/**
	 * Virtual Destructor
	 */
	virtual ~FWebBrowserSingleton();

	// IWebBrowserSingleton Interface
	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(void* OSWindowHandle, FString InitialURL, uint32 Width, uint32 Height, bool bUseTransparency) override;

	// FTickerObjectBase Interface
	virtual bool Tick(float DeltaTime) override;

	/**
	 * Gets the Current Locale Code in the format CEF expects
	 *
	 * @return Locale code as either "xx" or "xx-YY"
	 */
	static FString GetCurrentLocaleCode();

private:
#if WITH_CEF3
	/** Pointer to the CEF App implementation */
	CefRefPtr<FWebBrowserApp>			WebBrowserApp;
	/** List of currently existing browser windows */
	TArray<TWeakPtr<FWebBrowserWindow>>	WindowInterfaces;
#endif
};
