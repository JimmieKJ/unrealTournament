// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericWindow.h"
#include "SharedPointer.h"
#include "CocoaWindow.h"

/**
 * A platform specific implementation of FNativeWindow.
 * Native Windows provide platform-specific backing for and are always owned by an SWindow.
 */
class CORE_API FMacWindow : public FGenericWindow, public TSharedFromThis<FMacWindow>
{
public:
	~FMacWindow();

	static TSharedRef< FMacWindow > Make();

	FCocoaWindow* GetWindowHandle() const;

	void Initialize( class FMacApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FMacWindow >& InParent, const bool bShowImmediately );
	
	void OnDisplayReconfiguration(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags);

public:

	virtual void ReshapeWindow( int32 X, int32 Y, int32 Width, int32 Height ) override;

	virtual bool GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const override;

	virtual void MoveWindowTo ( int32 X, int32 Y ) override;

	virtual void BringToFront( bool bForce = false ) override;

	virtual void Destroy() override;

	virtual void Minimize() override;

	virtual void Maximize() override;

	virtual void Restore() override;

	virtual void Show() override;

	virtual void Hide() override;

	virtual void SetWindowMode( EWindowMode::Type NewWindowMode ) override;

	virtual EWindowMode::Type GetWindowMode() const override;

	virtual bool IsMaximized() const override;

	virtual bool IsMinimized() const override;

	virtual bool IsVisible() const override;

	virtual bool GetRestoredDimensions(int32& X, int32& Y, int32& Width, int32& Height) override;

	virtual void SetWindowFocus() override;

	virtual void SetOpacity( const float InOpacity ) override;

	virtual bool IsPointInWindow( int32 X, int32 Y ) const override;

	virtual int32 GetWindowBorderSize() const override;

	virtual void* GetOSWindowHandle() const  override { return WindowHandle; }

	virtual bool IsForegroundWindow() const override;

	virtual void SetText(const TCHAR* const Text) override;

	virtual void AdjustCachedSize( FVector2D& Size ) const override;

	/**
	 * Sets the window text - usually the title but can also be text content for things like controls
	 *
	 * @param Text	The window's title or content text
	 */
	bool IsRegularWindow() const;

	int32 PositionX;
	int32 PositionY;


private:

	/**
	 * Protect the constructor; only TSharedRefs of this class can be made.
	 */
	FMacWindow();


private:

	FMacApplication* OwningApplication;

	/** Mac window handle */
	FCocoaWindow* WindowHandle;

	NSRect PreFullscreenWindowRect;

	bool bIsVisible : 1;
	bool bIsClosed : 1;
};
