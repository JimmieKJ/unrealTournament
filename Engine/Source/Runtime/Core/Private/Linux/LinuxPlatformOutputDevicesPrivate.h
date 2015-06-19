// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OutputDeviceConsole.h"

class FOutputDeviceLinuxError : public FOutputDeviceError
{
public:

	/** Constructor, initializing member variables */
	CORE_API FOutputDeviceLinuxError();

	/**
	 * Serializes the passed in data unless the current event is suppressed.
	 *
	 * @param	Data	Text to log
	 * @param	Event	Event name used for suppression purposes
	 */
	virtual void Serialize(const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category) override;

	/**
	 * Error handling function that is being called from within the system wide global
	 * error handler, e.g. using structured exception handling on the PC.
	 */
	void HandleError() override;

private:

	int32		ErrorPos;
};

/**
* Linux implementation of console log window, just a printf to the terminal for now
*/
class CORE_API FOutputDeviceConsoleLinux : public FOutputDeviceConsole
{

private:

	/** true if the color is currently set by the caller */
	bool bOverrideColorSet;

public:

	/**
	 * Constructor, setting console control handler.
	 */
	FOutputDeviceConsoleLinux();
	~FOutputDeviceConsoleLinux();

	/**
	 * Shows or hides the console window.
	 *
	 * @param ShowWindow	Whether to show (true) or hide (false) the console window.
	 */
	virtual void Show(bool bShowWindow);

	/**
	 * Returns whether console is currently shown or not
	 *
	 * @return true if console is shown, false otherwise
	 */
	virtual bool IsShown();

	virtual bool IsAttached() {return false;}

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category);

};
