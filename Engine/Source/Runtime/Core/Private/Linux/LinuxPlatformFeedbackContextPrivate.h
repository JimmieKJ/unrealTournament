// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include <syslog.h>
#include "Misc/App.h"

#pragma once

/**
 * Feedback context implementation for Linux.
 */
class FFeedbackContextLinux : public FFeedbackContext
{
	/** Context information for warning and error messages */
	FContextSupplier*	Context;

public:
	// Variables.
	int32					SlowTaskCount;

	// Constructor.
	FFeedbackContextLinux()
	: FFeedbackContext()
	, Context( NULL )
	, SlowTaskCount( 0 )
	{}

	void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
	{
		// if we set the color for warnings or errors, then reset at the end of the function
		// note, we have to set the colors directly without using the standard SET_WARN_COLOR macro
		if (Verbosity==ELogVerbosity::Error || Verbosity==ELogVerbosity::Warning)
		{
			if (TreatWarningsAsErrors && Verbosity==ELogVerbosity::Warning)
			{
				Verbosity = ELogVerbosity::Error;
			}

			FString Prefix;
			if (Context)
			{
				Prefix = Context->GetContext() + TEXT(" : ");
			}
			FString Format = Prefix + FOutputDevice::FormatLogLine(Verbosity, Category, V);

			if (Verbosity == ELogVerbosity::Error)
			{
				// Only store off the message if running a commandlet.
				if (IsRunningCommandlet())
				{
					Errors.Add(Format);
				}
			}
			else
			{
				// Only store off the message if running a commandlet.
				if ( IsRunningCommandlet() )
				{
					Warnings.Add(Format);
				}
			}

			// send to syslog too (for zabbix etc)
			int BasePriority = (Verbosity == ELogVerbosity::Error) ? LOG_ERR : LOG_WARNING;
			syslog(BasePriority | LOG_USER, "%s", StringCast< char >(*Format).Get());
		}

		if( GLogConsole && IsRunningCommandlet() )
		{
			GLogConsole->Serialize( V, Verbosity, Category );
		}
		if( !GLog->IsRedirectingTo( this ) )
		{
			GLog->Serialize( V, Verbosity, Category );
		}
	}
	/** Ask the user a binary question, returning their answer */
	virtual bool YesNof( const FText& Question )
	{
		if( ( GIsClient || GIsEditor ) && ( ( GIsSilent != true ) && ( FApp::IsUnattended() != true ) ) )
		{
			//return( ::MessageBox( NULL, TempStr, *NSLOCTEXT("Core", "Question", "Question").ToString(), MB_YESNO|MB_TASKMODAL ) == IDYES);
			STUBBED("+++++++++++++++ LINUXPLATFORMFEEDBACKCONTEXTPRIVATE.H DIALOG BOX PROMPT +++++++++++++++++++");
			return true;
		}
		else
		{
			return false;
		}
	}

	void BeginSlowTask( const FText& Task, bool ShowProgressDialog, bool bShowCancelButton=false )
	{
		GIsSlowTask = ++SlowTaskCount>0;
	}
	void EndSlowTask()
	{
		check(SlowTaskCount>0);
		GIsSlowTask = --SlowTaskCount>0;
	}
	virtual bool StatusUpdate( int32 Numerator, int32 Denominator, const FText& StatusText )
	{
		return true;
	}
	FContextSupplier* GetContext() const
	{
		return Context;
	}
	void SetContext( FContextSupplier* InSupplier )
	{
		Context = InSupplier;
	}
};
