// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "IWebBrowserWindow.h"

enum class EWebBrowserDocumentState;
class FWebBrowserViewport;

class WEBBROWSER_API SWebBrowser
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWebBrowser)
		: _InitialURL(TEXT("www.google.com"))
		, _ShowControls(true)
		, _ShowAddressBar(false)
		, _ShowErrorMessage(true)
		, _SupportsTransparency(false)
		, _ViewportSize(FVector2D(320, 240))
	{ }

		/** A reference to the parent window. */
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)

		/** URL that the browser will initially navigate to. */
		SLATE_ARGUMENT(FString, InitialURL)

		/** Optional string to load contents as a web page. */
		SLATE_ARGUMENT(TOptional<FString>, ContentsToLoad)

		/** Whether to show standard controls like Back, Forward, Reload etc. */
		SLATE_ARGUMENT(bool, ShowControls)

		/** Whether to show an address bar. */
		SLATE_ARGUMENT(bool, ShowAddressBar)

		/** Whether to show an error message in case of loading errors. */
		SLATE_ARGUMENT(bool, ShowErrorMessage)

		/** Should this browser window support transparency. */
		SLATE_ARGUMENT(bool, SupportsTransparency)

		/** Desired size of the web browser viewport. */
		SLATE_ATTRIBUTE(FVector2D, ViewportSize);

		/** Called when document loading completed. */
		SLATE_EVENT(FSimpleDelegate, OnLoadCompleted)

		/** Called when document loading failed. */
		SLATE_EVENT(FSimpleDelegate, OnLoadError)

		/** Called when document loading started. */
		SLATE_EVENT(FSimpleDelegate, OnLoadStarted)

		/** Called when document title changed. */
		SLATE_EVENT(FOnTextChanged, OnTitleChanged)

		SLATE_EVENT(FOnTextChanged, OnUrlChanged)
		
		/** Called when a custom Javascript message is received from the browser process. */
		SLATE_EVENT(FOnJSQueryReceivedDelegate, OnJSQueryReceived)

		/** Called when a pending Javascript message has been canceled, either explicitly or by navigating away from the page containing the script. */
		SLATE_EVENT(FOnJSQueryCanceledDelegate, OnJSQueryCanceled)

		/** Called before a browse begins */
		SLATE_EVENT(FOnBeforeBrowseDelegate, OnBeforeBrowse)

		/** Called before a popup window happens */
		SLATE_EVENT(FOnBeforePopupDelegate, OnBeforePopup)

	SLATE_END_ARGS()


	/** Default constructor. */
	SWebBrowser();

	/**
	 * Construct the widget.
	 *
	 * @param InArgs  Declaration from which to construct the widget.
	 */
	void Construct(const FArguments& InArgs);

	void ExecuteJavascript(const FString& JS);

	/**
	 * Load the specified URL.
	 * 
	 * @param NewURL New URL to load.
	 */
	void LoadURL(FString NewURL);

	/**
	* Load a string as data to create a web page.
	*
	* @param Contents String to load.
	* @param DummyURL Dummy URL for the page.
	*/
	void LoadString(FString Contents, FString DummyURL);

	/** Get the current title of the web page. */
	FText GetTitleText() const;

	/**
	 * Gets the currently loaded URL.
	 *
	 * @return The URL, or empty string if no document is loaded.
	 */
	FString GetUrl() const;

	/**
	 * Gets the URL that appears in the address bar, this may not be the URL that is currently loaded in the frame.
	 *
	 * @return The address bar URL.
	 */
	FText GetAddressBarUrlText() const;

	/** Whether the document finished loading. */
	bool IsLoaded() const;

	/** Whether the document is currently being loaded. */
	bool IsLoading() const; 

private:

	/** Returns true if the browser can navigate backwards. */
	bool CanGoBack() const;

	/** Navigate backwards. */
	FReply OnBackClicked();

	/** Returns true if the browser can navigate forwards. */
	bool CanGoForward() const;

	/** Navigate forwards. */
	FReply OnForwardClicked();

	/** Get text for reload button depending on status */
	FText GetReloadButtonText() const;

	/** Reload or stop loading */
	FReply OnReloadClicked();

	/** Invoked whenever text is committed in the address bar. */
	void OnUrlTextCommitted( const FText& NewText, ETextCommit::Type CommitType );

	/** Get whether the page viewport should be visible */
	EVisibility GetViewportVisibility() const;

	/** Get whether loading throbber should be visible */
	EVisibility GetLoadingThrobberVisibility() const;

	/** Callback for document loading state changes. */
	void HandleBrowserWindowDocumentStateChanged(EWebBrowserDocumentState NewState);

	/** Callback to tell slate we want to update the contents of the web view based on changes inside the view. */
	void HandleBrowserWindowNeedsRedraw();

	/** Callback for document title changes. */
	void HandleTitleChanged(FString NewTitle);

	/** Callback for loaded url changes. */
	void HandleUrlChanged(FString NewUrl);
	
	/** Callback for received JS queries. */
	bool HandleJSQueryReceived(int64 QueryId, FString QueryString, bool Persistent, FJSQueryResultDelegate ResultDelegate);

	/** Callback for cancelled JS queries. */
	void HandleJSQueryCanceled(int64 QueryId);

	/** Callback for browse */
	bool HandleBeforeBrowse(FString URL, bool bIsRedirect);

	bool HandleBeforePopup(FString URL, FString Target);

private:

	/** Interface for dealing with a web browser window. */
	TSharedPtr<IWebBrowserWindow> BrowserWindow;

	/** Viewport interface for rendering the web page. */
	TSharedPtr<FWebBrowserViewport> BrowserViewport;

	/** The url that appears in the address bar which can differ from the url of the loaded page */
	FText AddressBarUrl;

	/** Editable text widget used for an address bar */
	TSharedPtr< SEditableTextBox > InputText;

	/** A delegate that is invoked when document loading completed. */
	FSimpleDelegate OnLoadCompleted;

	/** A delegate that is invoked when document loading failed. */
	FSimpleDelegate OnLoadError;

	/** A delegate that is invoked when document loading started. */
	FSimpleDelegate OnLoadStarted;

	/** A delegate that is invoked when document title changed. */
	FOnTextChanged OnTitleChanged;

	/** A delegate that is invoked when document address changed. */
	FOnTextChanged OnUrlChanged;
	
	/** A delegate that is invoked when render process Javascript code sends a query message to the client. */
	FOnJSQueryReceivedDelegate OnJSQueryReceived;

	/** A delegate that is invoked when render process cancels an ongoing query. Handler must clean up corresponding result delegate. */
	FOnJSQueryCanceledDelegate OnJSQueryCanceled;

	/** a delegate that is invoked when the browser browses */
	FOnBeforeBrowseDelegate OnBeforeBrowse;

	/** a delegate that is invoked when the browser attempts to pop up a new window */
	FOnBeforePopupDelegate OnBeforePopup;

	/** A flag to avoid having more than one active timer delegate in flight at the same time */
	bool IsHandlingRedraw;
};
