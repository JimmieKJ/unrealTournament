// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacPlatformOutputDevicesPrivate.h: Mac implementations of output devices
=============================================================================*/

#pragma once

@interface FMacConsoleWindow : NSWindow<NSWindowDelegate>
@end

class FOutputDeviceMacError : public FOutputDeviceError
{
public:
	/** Constructor, initializing member variables */
	CORE_API FOutputDeviceMacError();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize( const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category ) override;

	/**
	 * Error handling function that is being called from within the system wide global
	 * error handler, e.g. using structured exception handling on the PC.
	 */
	virtual void HandleError() override;

private:
	int32		ErrorPos;
};

/**
 * Mac implementation of console log window, utilizing the Win32 console API
 */
class CORE_API FOutputDeviceConsoleMac : public FOutputDeviceConsole
{
private:
	/** Handle to the console log window */
	FMacConsoleWindow* ConsoleHandle;

	/** Handle to window's content view */
	NSTextView* TextView;

	/** Handle to window's scroll view */
	NSScrollView* ScrollView;

	/** Handle text color of TextView */
	NSDictionary* TextViewTextColor;

	/** Critical section for Serialize() */
	FCriticalSection CriticalSection;

	/** Number of outstanding dispatched Cocoa tasks */
	uint64 OutstandingTasks;

	/**
	 * Saves the console window's position and size to the game .ini
	 */
	void SaveToINI();

	/**
	 * Creates console window
	 */
	void CreateConsole();

	/**
	 * Destroys console window
	 */
	void DestroyConsole();

	/**
	 *
	 */
	void SetDefaultTextColor();

public:

	/** 
	 * Constructor, setting console control handler.
	 */
	FOutputDeviceConsoleMac();
	~FOutputDeviceConsoleMac();

	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (true) or hide (false) the console window.
	 */
	virtual void Show( bool ShowWindow );

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return true if console is shown, false otherwise
	 */
	virtual bool IsShown();

	virtual bool IsAttached() { return false; }

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category );
};
