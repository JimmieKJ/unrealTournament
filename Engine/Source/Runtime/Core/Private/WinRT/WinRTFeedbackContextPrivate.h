// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WinRTFeedbackContextPrivate.h: Unreal WinRT user interface interaction.
=============================================================================*/

#pragma once
#include "Misc/App.h"

/**
 * Feedback context implementation for WinRT.
 */
class FFeedbackContextWinRT : public FFeedbackContext
{
	/** Context information for warning and error messages */
	FContextSupplier*	Context;

public:

	// Constructor.
	FFeedbackContextWinRT()
		: FFeedbackContext()
		, Context( NULL )
	{}

	void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
	{
		// if we set the color for warnings or errors, then reset at the end of the function
		// note, we have to set the colors directly without using the standard SET_WARN_COLOR macro
		bool bNeedToResetColor = false;
		if( Verbosity==ELogVerbosity::Error || Verbosity==ELogVerbosity::Warning )
		{
			if( TreatWarningsAsErrors && Verbosity==ELogVerbosity::Warning )
			{
				Verbosity = ELogVerbosity::Error;
			}

			FString Prefix;
			if( Context )
			{
				Prefix = Context->GetContext() + TEXT(" : ");
			}
			FString Format = Prefix + FOutputDevice::FormatLogLine(Verbosity, Category, V);

			if(Verbosity == ELogVerbosity::Error)
			{
				Serialize(TEXT(""), ELogVerbosity::SetColor, Category); bNeedToResetColor = true;
				// Only store off the message if running a commandlet.
				if ( IsRunningCommandlet() )
				{
					Errors.Add(Format);
				}
			}
			else
			{
				Serialize(TEXT(""), ELogVerbosity::SetColor, Category); bNeedToResetColor = true;
				// Only store off the message if running a commandlet.
				if ( IsRunningCommandlet() )
				{
					Warnings.Add(Format);
				}
			}
		}

		if( GLogConsole && IsRunningCommandlet() )
		{
			GLogConsole->Serialize( V, Verbosity, Category );
		}
		if( !GLog->IsRedirectingTo( this ) )
		{
			GLog->Serialize( V, Verbosity, Category );
		}

		if (bNeedToResetColor)
		{
			Serialize(TEXT(""), ELogVerbosity::SetColor, Category);
		}
	}

	VARARG_BODY( bool, YesNof, const TCHAR*, VARARG_NONE )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
		if( ( GIsClient || GIsEditor ) && ( ( GIsSilent != true ) && ( FApp::IsUnattended() != true ) ) )
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TempStr);
			return false;
		}
		else
		{
			return false;
		}
	}

	VARARG_BODY( bool VARARGS, StatusUpdatef, const TCHAR*, VARARG_EXTRA(int32 Numerator) VARARG_EXTRA(int32 Denominator) )
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
