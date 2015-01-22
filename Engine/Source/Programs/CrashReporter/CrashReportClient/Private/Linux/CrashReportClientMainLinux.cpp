// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "LinuxPlatformCrashContext.h"
#include <locale.h>

extern int32 ReportCrash(const FLinuxCrashContext& Context);	// FIXME: handle expose it someplace else?

/**
 * Because crash reporters can crash, too
 */
void CrashReporterCrashHandler(const FGenericCrashContext& GenericContext)
{
	const FLinuxCrashContext& Context = static_cast< const FLinuxCrashContext& >( GenericContext );

	printf("CrashHandler: Signal=%d\n", Context.Signal);
	ReportCrash(Context);
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
		GError->HandleError();
	}
	FPlatformMisc::RequestExit(true);
}


/**
 * main(), called when the application is started
 */
int main(int argc, const char *argv[])
{
	FPlatformMisc::SetGracefulTerminationHandler();
	FPlatformMisc::SetCrashHandler(CrashReporterCrashHandler);

	FString SavedCommandLine;

	setlocale(LC_CTYPE, "");
	for (int32 Option = 1; Option < argc; Option++)
	{
		SavedCommandLine += TEXT(" ");
		SavedCommandLine += UTF8_TO_TCHAR(argv[Option]);	// note: technically it depends on locale
	}

	// Run the app
	RunCrashReportClient(*SavedCommandLine);

	return 0;
}
