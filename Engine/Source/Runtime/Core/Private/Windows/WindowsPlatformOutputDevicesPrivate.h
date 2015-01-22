// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Misc/OutputDeviceConsole.h"

#ifndef WANTS_WINDOWS_EVENT_LOGGING
	#if UE_SERVER
		#define WANTS_WINDOWS_EVENT_LOGGING 1
	#else
		#define WANTS_WINDOWS_EVENT_LOGGING 0
	#endif
#endif


#if WANTS_WINDOWS_EVENT_LOGGING

/**
 * Output device that writes to Windows Event Log
 */
class FOutputDeviceEventLog
	: public FOutputDevice
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
		if (FParse::Value(FCommandLine::Get(),TEXT("-Login="),ServerName))
		{
			InstanceName = FApp::GetGameName();
			InstanceName += ServerName;
		}
		else
		{
			uint32 ProcID = GetCurrentProcessId();
			InstanceName = FString::Printf(TEXT("%s-PID%d"),FApp::GetGameName(),ProcID);
		}
		// Open the event log using the name built above
		EventLog = RegisterEventSource(NULL,*InstanceName);
		if (EventLog == NULL)
		{
			UE_LOG(LogWindows, Error,TEXT("Failed to open the Windows Event Log for writing (%d)"),GetLastError());
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
#endif //WANTS_WINDOWS_EVENT_LOGGING

class FOutputDeviceWindowsError : public FOutputDeviceError
{
public:
	/** Constructor, initializing member variables */
	CORE_API FOutputDeviceWindowsError();

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
};

/**
 * Windows implementation of console log window, utilizing the Win32 console API
 */
class CORE_API FOutputDeviceConsoleWindows : public FOutputDeviceConsole
{
private:
	/** Handle to the console log window */
	HANDLE ConsoleHandle;

	/** true if the color is currently set by the caller */
	bool OverrideColorSet;

	/** true if this console device is attached to an existing console window. */
	bool bIsAttached;
	/**
	 * Saves the console window's position and size to the game .ini
	 */
	void SaveToINI();

public:

	/** 
	 * Constructor, setting console control handler.
	 */
	FOutputDeviceConsoleWindows();
	~FOutputDeviceConsoleWindows();

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

	virtual bool IsAttached();

	/**
	 * Displays text on the console and scrolls if necessary.
	 *
	 * @param Data	Text to display
	 * @param Event	Event type, used for filtering/ suppression
	 */
	void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category );

	void SetColor( const TCHAR* Color );
};


