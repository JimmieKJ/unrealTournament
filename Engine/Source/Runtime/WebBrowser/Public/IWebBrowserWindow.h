// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Forward Declarations
struct FGeometry;
struct FKeyEvent;
struct FCharacterEvent;
struct FPointerEvent;
class FSlateShaderResource;

DECLARE_MULTICAST_DELEGATE_OneParam( FOnTitleChangedDelegate, FString );

/**
 * Interface for dealing with a Web Browser window
 */
class IWebBrowserWindow
{
public:
	/**
	 * Set the desired size of the web browser viewport
	 * 
	 * @param WindowSize Desired viewport size
	 */
	virtual void SetViewportSize(FVector2D WindowSize) = 0;

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

	/**
	 * Gets the current title of the Browser page
	 */
	virtual FString GetTitle() const = 0;

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

	/**
	 * Called when Capture lost
	 */
	virtual void OnCaptureLost() = 0;

	/**
	 * Returns true if the browser can navigate backwards.
	 */
	virtual bool CanGoBack() const = 0;

	/**
	 * Navigate backwards.
	 */
	virtual void GoBack() = 0;

	/**
	 * Returns true if the browser can navigate forwards.
	 */
	virtual bool CanGoForward() const = 0;

	/**
	 * Navigate forwards.
	 */
	virtual void GoForward() = 0;

	/**
	 * Returns true if the browser is currently loading.
	 */
	virtual bool IsLoading() const = 0;

	/**
	 * Reload the current page.
	 */
	virtual void Reload() = 0;

	/**
	 * Stop loading the page.
	 */
	virtual void StopLoad() = 0;

	/**
	 * Accesses a delegate to allow callbacks when a browser title changes
	 *
	 * @return A multicast delegate that you can register with
	 */
	FOnTitleChangedDelegate& OnTitleChanged() {return OnTitleChangedDelegate;}

protected:

	/**
	 * Virtual Destructor
	 */
	virtual ~IWebBrowserWindow() {};

	/** Delegate for broadcasting title changes */
	FOnTitleChangedDelegate OnTitleChangedDelegate;
};
