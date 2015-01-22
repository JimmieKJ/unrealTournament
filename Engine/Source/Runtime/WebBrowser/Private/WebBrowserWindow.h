// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3

#include "IWebBrowserWindow.h"
#include "WebBrowserHandler.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#include "include/internal/cef_ptr.h"
#include "include/cef_render_handler.h"
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

// Forward Declarations
struct FInputEvent;

class FSlateRenderer;
class FSlateUpdatableTexture;

/**
 * Implementation of interface for dealing with a Web Browser window
 */
class FWebBrowserWindow : public IWebBrowserWindow, public TSharedFromThis<FWebBrowserWindow>
{
public:
	/**
	 * Default Constructor
	 * 
	 * @param InViewportSize Initial size of the browser window
	 */
	FWebBrowserWindow(FIntPoint InViewportSize);
	/**
	 * Virtual Destructor
	 */
	virtual ~FWebBrowserWindow();

	// IWebBrowserWindow Interface
	virtual void SetViewportSize(FVector2D WindowSize) override;
	virtual FSlateShaderResource* GetTexture() override;
	virtual bool IsValid() const override;
	virtual bool HasBeenPainted() const override;
	virtual bool IsClosing() const override;
	virtual FString GetTitle() const override;
	virtual void OnKeyDown(const FKeyEvent& InKeyEvent) override;
	virtual void OnKeyUp(const FKeyEvent& InKeyEvent) override;
	virtual void OnKeyChar(const FCharacterEvent& InCharacterEvent) override;
	virtual void OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnFocus(bool SetFocus) override;
	virtual void OnCaptureLost() override;
	virtual bool CanGoBack() const override;
	virtual void GoBack() override;
	virtual bool CanGoForward() const override;
	virtual void GoForward() override;
	virtual bool IsLoading() const override;
	virtual void Reload() override;
	virtual void StopLoad() override;

	/**
	 * Set the CEF Handler receiving browser callbacks for this window
	 *
	 * @param InHandler Pointer to the handler for this window
	 */
	void SetHandler(CefRefPtr<FWebBrowserHandler> InHandler);

	/**
	 * Close this window so that it can no longer be used
	 */
	void CloseBrowser();

private:
	/**
	 * Called to pass reference to the underlying CefBrowser as this is not created at the same time
	 * as the FWebBrowserWindow
	 *
	 * @param Browser The CefBrowser for this window
	 */
	void BindCefBrowser(CefRefPtr<CefBrowser> Browser);

	/**
	 * Sets the Title of this window
	 * 
	 * @param InTitle The new title of this window
	 */
	void SetTitle(const CefString& InTitle);

	/**
	 * Get the current proportions of this window
	 *
	 * @param Rect Reference to CefRect to store sizes
	 * @return Whether Rect was set up correctly
	 */
	bool GetViewRect(CefRect& Rect);

	/**
	 * Called when there is an update to the rendered web page
	 *
	 * @param Type Paint type
	 * @param DirtyRects List of image areas that have been changed
	 * @param Buffer Pointer to the raw texture data
	 * @param Width Width of the texture
	 * @param Height Height of the texture
	 */
	void OnPaint(CefRenderHandler::PaintElementType Type, const CefRenderHandler::RectList& DirtyRects, const void* Buffer, int Width, int Height);
	
	/**
	 * Called when cursor would change due to web browser interaction
	 * 
	 * @param Cursor Handle to CEF mouse cursor
	 */
	void OnCursorChange(CefCursorHandle Cursor);

	/**
	 * Gets the Cef Keyboard Modifiers based on a Key Event
	 * 
	 * @param KeyEvent The Key event
	 * @return Bits representing keyboard modifiers
	 */
	static int32 GetCefKeyboardModifiers(const FKeyEvent& KeyEvent);

	/**
	 * Gets the Cef Mouse Modifiers based on a Mouse Event
	 * 
	 * @param InMouseEvent The Mouse event
	 * @return Bits representing mouse modifiers
	 */
	static int32 GetCefMouseModifiers(const FPointerEvent& InMouseEvent);

	/**
	 * Gets the Cef Input Modifiers based on an Input Event
	 * 
	 * @param InputEvent The Input event
	 * @return Bits representing input modifiers
	 */
	static int32 GetCefInputModifiers(const FInputEvent& InputEvent);

	/** Interface to the texture we are rendering to */
	FSlateUpdatableTexture*			UpdatableTexture;
	/** Temporary storage for the raw texture data */
	TArray<uint8>					TextureData;
	/** Pointer to the CEF Handler for this window */
	CefRefPtr<FWebBrowserHandler>	Handler;
	/** Pointer to the CEF Browser for this window */
	CefRefPtr<CefBrowser>			InternalCefBrowser;
	/** Current title of this window */
	FString							Title;
	/** Current size of this window */
	FIntPoint						ViewportSize;
	/** Whether this window is closing */
	bool							bIsClosing;
	/** Whether this window has been painted at least once */
	bool							bHasBeenPainted;

	// Allow the Handler to access functions only it needs
	friend class FWebBrowserHandler;
};

#endif
