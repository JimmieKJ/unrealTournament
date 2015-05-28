// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Forward Declarations
class FSlateRenderer;
class IWebBrowserWindow;

/**
 * A singleton class that takes care of general web browser tasks
 */
class WEBBROWSER_API IWebBrowserSingleton
{
public:
	/**
	 * Virtual Destructor
	 */
	virtual ~IWebBrowserSingleton() {};

	/**
	 * Create a new web browser window
	 *
	 * @param OSWindowHandle Handle of OS Window that the browser will be displayed in (can be null)
	 * @param InitialURL URL that the browser should initially navigate to
	 * @param Width Initial width of the browser
	 * @param Height Initial height of the browser
	 * @param bUseTransparency Whether to allow transparent rendering of pages
	 * @param ContentsToLoad Optional string to load as a web page
	 * @param ShowErrorMessage Whether to show an error message in case of loading errors.
	 * @return New Web Browser Window Interface (may be null if not supported)
	 */
	virtual TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(void* OSWindowHandle, FString InitialURL, uint32 Width, uint32 Height, bool bUseTransparency, TOptional<FString> ContentsToLoad = TOptional<FString>(), bool ShowErrorMessage = true) = 0;
};
