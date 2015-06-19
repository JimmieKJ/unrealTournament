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


class FWebBrowserApp;
class FWebBrowserHandler;
class FWebBrowserWindow;


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

	TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(void* OSWindowHandle, FString InitialURL, uint32 Width, uint32 Height, bool bUseTransparency, TOptional<FString> ContentsToLoad = TOptional<FString>(), bool ShowErrorMessage = true) override;

public:

	// FTickerObjectBase Interface

	virtual bool Tick(float DeltaTime) override;

private:
#if WITH_CEF3
	/** Pointer to the CEF App implementation */
	CefRefPtr<FWebBrowserApp>			WebBrowserApp;
	/** List of currently existing browser windows */
	TArray<TWeakPtr<FWebBrowserWindow>>	WindowInterfaces;
#endif
};
