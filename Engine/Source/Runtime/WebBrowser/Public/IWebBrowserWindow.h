// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


struct FGeometry;
struct FKeyEvent;
struct FCharacterEvent;
struct FPointerEvent;
class FSlateShaderResource;


enum class EWebBrowserDocumentState
{
	Completed,
	Error,
	Loading,
	NoDocument
};


/**
 * Interface for dealing with a Web Browser window
 */
class IWebBrowserWindow
{
public:

	/**
	 * Load the specified URL
	 *
	 * @param NewURL New URL to load
	 */
	virtual void LoadURL(FString NewURL) = 0;

	/**
	 * Load a string as data to create a web page
	 *
	 * @param Contents String to load
	 * @param DummyURL Dummy URL for the page
	 */
	virtual void LoadString(FString Contents, FString DummyURL) = 0;

	/**
	 * Set the desired size of the web browser viewport
	 * 
	 * @param WindowSize Desired viewport size
	 */
	virtual void SetViewportSize(FIntPoint WindowSize) = 0;

	/**
	 * Gets interface to the texture representation of the browser
	 *
	 * @return A slate shader resource that can be rendered
	 */
	virtual FSlateShaderResource* GetTexture() = 0;

	/**
	 * Checks whether the web browser is valid and ready for use
	 */
	virtual bool IsValid() const = 0;

	/**
	 * Checks whether the web browser has been painted at least once
	 */
	virtual bool HasBeenPainted() const = 0;

	/**
	 * Checks whether the web browser is currently being shut down
	 */
	virtual bool IsClosing() const = 0;

	/** Gets the loading state of the current document. */
	virtual EWebBrowserDocumentState GetDocumentLoadingState() const = 0;

	/**
	 * Gets the current title of the Browser page
	 */
	virtual FString GetTitle() const = 0;

	/**
	 * Gets the currently loaded URL.
	 *
	 * @return The URL, or empty string if no document is loaded.
	 */
	virtual FString GetUrl() const = 0;

	/**
	 * Notify the browser that a key has been pressed
	 *
	 * @param  InKeyEvent  Key event
	 */
	virtual void OnKeyDown(const FKeyEvent& InKeyEvent) = 0;

	/**
	 * Notify the browser that a key has been released
	 *
	 * @param  InKeyEvent  Key event
	 */
	virtual void OnKeyUp(const FKeyEvent& InKeyEvent) = 0;

	/**
	 * Notify the browser of a character event
	 *
	 * @param InCharacterEvent Character event
	 */
	virtual void OnKeyChar(const FCharacterEvent& InCharacterEvent) = 0;

	/**
	 * Notify the browser that a mouse button was pressed within it
	 *
	 * @param MyGeometry The Geometry of the browser
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;

	/**
	 * Notify the browser that a mouse button was released within it
	 *
	 * @param MyGeometry The Geometry of the browser
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;

	/**
	 * Notify the browser of a double click event
	 *
	 * @param MyGeometry The Geometry of the browser
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;

	/**
	 * Notify the browser that a mouse moved within it
	 *
	 * @param MyGeometry The Geometry of the browser
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;

	/**
	 * Called when the mouse wheel is spun
	 *
	 * @param MyGeometry The Geometry of the browser
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;

	/**
	 * Called when browser receives/loses focus
	 */
	virtual void OnFocus(bool SetFocus) = 0;

	/** Called when Capture lost */
	virtual void OnCaptureLost() = 0;

	/**
	 * Returns true if the browser can navigate backwards.
	 */
	virtual bool CanGoBack() const = 0;

	/** Navigate backwards. */
	virtual void GoBack() = 0;

	/**
	 * Returns true if the browser can navigate forwards.
	 */
	virtual bool CanGoForward() const = 0;

	/** Navigate forwards. */
	virtual void GoForward() = 0;

	/**
	 * Returns true if the browser is currently loading.
	 */
	virtual bool IsLoading() const = 0;

	/** Reload the current page. */
	virtual void Reload() = 0;

	/** Stop loading the page. */
	virtual void StopLoad() = 0;

	virtual void ExecuteJavascript(const FString&) = 0;

public:

	/** A delegate that is invoked when the loading state of a document changed. */
	DECLARE_EVENT_OneParam(IWebBrowserWindow, FOnDocumentStateChanged, EWebBrowserDocumentState /*NewState*/);
	virtual FOnDocumentStateChanged& OnDocumentStateChanged() = 0;

	/** A delegate to allow callbacks when a browser title changes. */
	DECLARE_EVENT_OneParam(IWebBrowserWindow, FOnTitleChanged, FString /*NewTitle*/);
	virtual FOnTitleChanged& OnTitleChanged() = 0;

	/** A delegate to allow callbacks when a frame url changes. */
	DECLARE_EVENT_OneParam(IWebBrowserWindow, FOnUrlChanged, FString /*NewUrl*/);
	virtual FOnUrlChanged& OnUrlChanged() = 0;

	/** A delegate that is invoked when the off-screen window has been repainted and requires an update. */
	DECLARE_EVENT(IWebBrowserWindow, FOnNeedsRedraw)
	virtual FOnNeedsRedraw& OnNeedsRedraw() = 0;

	/** A delegate that is invoked when JS code sends a query to the front end. The result delegate can either be executed immediately or saved and executed later (multiple times if the boolean persistent argument is true).
	 * The arguments are an integer error code (0 for success) and a reply string. If you need pass more complex data to the JS code, you will have to pack the data in some way (such as JSON encoding it). */
	DECLARE_DELEGATE_RetVal_FourParams(bool, FONJSQueryReceived, int64, FString, bool, FJSQueryResultDelegate)
	virtual FONJSQueryReceived& OnJSQueryReceived() = 0;

	/** A delegate that is invoked when an outstanding query is canceled. Implement this if you are saving delegates passed to OnQueryReceived. */
	DECLARE_DELEGATE_OneParam(FONJSQueryCanceled, int64)
	virtual FONJSQueryCanceled& OnJSQueryCanceled() = 0;

	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnBeforeBrowse, FString, bool)
	virtual FOnBeforeBrowse& OnBeforeBrowse() = 0;

	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnBeforePopup, FString, FString)
	virtual FOnBeforePopup& OnBeforePopup() = 0;

protected:

	/** Virtual Destructor. */
	virtual ~IWebBrowserWindow() { };
};
