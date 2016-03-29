// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// Forward Declarations
class FSlateRenderer;
class IWebBrowserWindow;
class FWebBrowserWindow;
struct FWebBrowserWindowInfo;

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
	 * @param BrowserWindowParent The parent browser window
	 * @param BrowserWindowInfo Info for setting up the new browser window
	 * @return New Web Browser Window Interface (may be null if not supported)
	 */
	virtual TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		TSharedPtr<FWebBrowserWindow>& BrowserWindowParent,
		TSharedPtr<FWebBrowserWindowInfo>& BrowserWindowInfo
		) = 0;

	/**
	 * Create a new web browser window
	 *
	 * @param OSWindowHandle Handle of OS Window that the browser will be displayed in (can be null)
	 * @param InitialURL URL that the browser should initially navigate to
	 * @param bUseTransparency Whether to allow transparent rendering of pages
	 * @param ContentsToLoad Optional string to load as a web page
	 * @param ShowErrorMessage Whether to show an error message in case of loading errors.
	 * @param BackgroundColor Opaque background color used before a document is loaded and when no document color is specified.
	 * @return New Web Browser Window Interface (may be null if not supported)
	 */
	virtual TSharedPtr<IWebBrowserWindow> CreateBrowserWindow(
		void* OSWindowHandle, 
		FString InitialURL, 
		bool bUseTransparency,
		bool bThumbMouseButtonNavigation,
		TOptional<FString> ContentsToLoad = TOptional<FString>(), 
		bool ShowErrorMessage = true,
		FColor BackgroundColor = FColor(255, 255, 255, 255)) = 0;

	/**
	 * Delete all browser cookies.
	 *
	 * Removes all matching cookies. Leave both URL and CookieName blank to delete the entire cookie database.
	 * The actual deletion will be scheduled on the browser IO thread, so the operation may not have completed when the function returns.
	 *
	 * @param URL The base URL to match when searching for cookies to remove. Use blank to match all URLs.
	 * @param CookieName The name of the cookie to delete. If left unspecified, all cookies will be removed.
	 * @param Completed A callback function that will be invoked asynchronously on the game thread when the deletion is complete passing in the number of deleted objects.
	 */
	virtual void DeleteBrowserCookies(FString URL = TEXT(""), FString CookieName = TEXT(""), TFunction<void (int)> Completed = nullptr) = 0;

	/**
	 * Enable or disable CTRL/CMD-SHIFT-I shortcut to show the Chromium Dev tools window.
	 * The value defaults to true on debug builds, otherwise false.
	 *
	 * The relevant handlers for spawning new browser windows have to be set up correctly in addition to this flag being true before anything is shown.
	 *
	 * @param Value a boolean value to enable or disable the keyboard shortcut.
	 */
	virtual void SetDevToolsShortcutEnabled(bool Value) = 0;


	/**
	 * Returns wether the CTRL/CMD-SHIFT-I shortcut to show the Chromium Dev tools window is enabled.
	 *
	 * The relevant handlers for spawning new browser windows have to be set up correctly in addition to this flag being true before anything is shown.
	 *
	 * @return a boolean value indicating whether the keyboard shortcut is enabled or not.
	 */
	virtual bool IsDevToolsShortcutEnabled() = 0;

};
