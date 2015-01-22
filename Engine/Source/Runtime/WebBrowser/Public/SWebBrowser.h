// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

class IWebBrowserWindow;
class FWebBrowserViewport;

class WEBBROWSER_API SWebBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWebBrowser)
		: _InitialURL(TEXT("www.google.com"))
		, _ShowControls(true)
		, _SupportsTransparency(false)
		, _ViewportSize(FVector2D(320, 240))
		{}

		/** A reference to the parent window */
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)

		/** URL that the browser will initially navigate to */
		SLATE_ARGUMENT(FString, InitialURL)

		/** Whether to show standard controls like Back, Forward, Reload etc. */
		SLATE_ARGUMENT(bool, ShowControls)

		/** Should this browser window support transparency */
		SLATE_ARGUMENT(bool, SupportsTransparency)

		/** Desired size of the web browser viewport */
		SLATE_ATTRIBUTE(FVector2D, ViewportSize);
	SLATE_END_ARGS()

	/**
	 * Constructor
	 */
	SWebBrowser();

	/**
	 * Construct the widget.
	 *
	 * @param InArgs  Declaration from which to construct the widget.
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Get the current title of the web page
	 */
	FText GetTitleText() const;

private:
	/**
	 * Returns true if the browser can navigate backwards.
	 */
	bool CanGoBack() const;

	/**
	 * Navigate backwards.
	 */
	FReply OnBackClicked();

	/**
	 * Returns true if the browser can navigate forwards.
	 */
	bool CanGoForward() const;

	/**
	 * Navigate forwards.
	 */
	FReply OnForwardClicked();

	/**
	 * Get text for reload button depending on status
	 */
	FText GetReloadButtonText() const;

	/**
	 * Reload or stop loading
	 */
	FReply OnReloadClicked();

	/**
	 * Get whether the page viewport should be visible
	 */
	EVisibility GetViewportVisibility() const;

	/**
	 * Get whether loading throbber should be visible
	 */
	EVisibility GetLoadingThrobberVisibility() const;

private:
	/** Interface for dealing with a web browser window */
	TSharedPtr<IWebBrowserWindow>	BrowserWindow;

	/** Viewport interface for rendering the web page */
	TSharedPtr<FWebBrowserViewport>	BrowserViewport;
};
