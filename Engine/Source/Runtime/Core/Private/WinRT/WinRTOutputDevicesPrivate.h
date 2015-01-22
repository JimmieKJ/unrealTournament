// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WinRTOutputDevicesPrivate.h: WinRT implementations of output devices
=============================================================================*/

#pragma once

#ifndef WANTS_WINRT_EVENT_LOGGING
	#define WANTS_WINRT_EVENT_LOGGING 0
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogWinRTOutputDevices, Log, All);

#if WANTS_WINRT_EVENT_LOGGING
/**
 * Output device that writes to WinRT Event Log
 */
class FOutputDeviceEventLog :
	public FOutputDevice
{
	/** Handle to the event log object */
	HANDLE EventLog;

public:
	/**
	 * Constructor, initializing member variables
	 */
	FOutputDeviceEventLog()
		: EventLog(NULL)
	{
		FString InstanceName;
		FString ServerName;
		// Build a name to uniquely identify this instance
		if (Parse(appCmdLine(),TEXT("-Login="),ServerName))
		{
			InstanceName = appGetGameName();
			InstanceName += ServerName;
		}
		else
		{
			uint32 ProcID = GetCurrentProcessId();
			InstanceName = FString::Printf(TEXT("%s-PID%d"),appGetGameName(),ProcID);
		}
		// Open the event log using the name built above
		EventLog = RegisterEventSource(NULL,*InstanceName);
		if (EventLog == NULL)
		{
			UE_LOG(LogWinRTOutputDevices, Error,TEXT("Failed to open the WinRT Event Log for writing (%d)"),GetLastError());
		}
	}

	/** Destructor that cleans up any remaining resources */
	virtual ~FOutputDeviceEventLog()
	{
		TearDown();
	}

	virtual void Serialize(const TCHAR* Buffer, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (EventLog != NULL)
		{
			// Only forward errors and warnings to the event log
			switch (Verbosity)
			{
			case ELogVerbosity::Error:
				{
					ReportEvent(EventLog,
						EVENTLOG_ERROR_TYPE,
						NULL,
						0xC0000001L,
						NULL,
						1,
						0,
						&Buffer,
						NULL);
					break;
				}
			case ELogVerbosity::Warning:
				{
					ReportEvent(EventLog,
						EVENTLOG_WARNING_TYPE,
						NULL,
						0x80000002L,
						NULL,
						1,
						0,
						&Buffer,
						NULL);
					break;
				}
			}
		}
	}
	
	/** Does nothing */
	virtual void Flush(void)
	{
	}

	/**
	 * Closes any event log handles that are open
	 */
	virtual void TearDown(void)
	{
		if (EventLog != NULL)
		{
			DeregisterEventSource(EventLog);
			EventLog = NULL;
		}
	}
};
#endif //WANTS_WINRT_EVENT_LOGGING

class FOutputDeviceWinRTError : public FOutputDeviceError
{
public:
	/** Constructor, initializing member variables */
	CORE_API FOutputDeviceWinRTError();

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
	void HandleError();

private:
	int32		ErrorPos;
};

/**
 * WinRT implementation of console log window, utilizing the Win32 console API
 */
class CORE_API FOutputDeviceConsoleWinRT : public FOutputDeviceConsole
{
private:
	/** Handle to the console log window */
	HANDLE ConsoleHandle;

	/**
	 * Saves the console window's position and size to the game .ini
	 */
	void SaveToINI();

public:

	/** 
	 * Constructor, setting console control handler.
	 */
	FOutputDeviceConsoleWinRT();
	~FOutputDeviceConsoleWinRT();

	/**
	 * Shows or hides the console window. 
	 *
	 * @param ShowWindow	Whether to show (true) or hide (false) the console window.
	 */
	virtual void Show(bool ShowWindow);

	/** 
	 * Returns whether console is currently shown or not
	 *
	 * @return true if console is shown, false otherwise
	 */
	virtual bool IsShown();

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category );
};
