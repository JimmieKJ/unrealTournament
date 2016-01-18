// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "CrashReportClientUnattended.h"
#include "CrashReportUtil.h"
#include "CrashDescription.h"

FCrashReportClientUnattended::FCrashReportClientUnattended(FPlatformErrorReport& ErrorReport)
	: Uploader( FCrashReportClientConfig::Get().GetReceiverAddress() )
{
	ErrorReport.TryReadDiagnosticsFile();

	// Process the report synchronously
	ErrorReport.DiagnoseReport();

	// Update properties for the crash.
	ErrorReport.SetPrimaryCrashProperties( *FPrimaryCrashProperties::Get() );

	Uploader.BeginUpload( ErrorReport );
	StartTicker();
}

bool FCrashReportClientUnattended::Tick(float UnusedDeltaTime)
{
	if (Uploader.IsFinished())
	{
		FPlatformMisc::RequestExit(false /* don't force */);
		// No more ticks, thank you
		return false;
	}

	return true;	
}

void FCrashReportClientUnattended::StartTicker()
{
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCrashReportClientUnattended::Tick), 1.f);
}
