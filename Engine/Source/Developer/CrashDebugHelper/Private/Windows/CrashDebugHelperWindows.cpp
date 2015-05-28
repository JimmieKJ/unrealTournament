// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "CrashDebugHelperWindows.h"
#include "WindowsPlatformStackWalkExt.h"

#include "EngineVersion.h"
#include "ISourceControlModule.h"

#include "AllowWindowsPlatformTypes.h"
#include <DbgHelp.h>

FCrashDebugHelperWindows::FCrashDebugHelperWindows()
{
}

FCrashDebugHelperWindows::~FCrashDebugHelperWindows()
{
}


bool FCrashDebugHelperWindows::ParseCrashDump(const FString& InCrashDumpName, FCrashDebugInfo& OutCrashDebugInfo)
{
	return true;
}

bool FCrashDebugHelperWindows::CreateMinidumpDiagnosticReport( const FString& InCrashDumpFilename )
{
	const bool bSyncSymbols = FParse::Param( FCommandLine::Get(), TEXT( "SyncSymbols" ) );
	const bool bAnnotate = FParse::Param( FCommandLine::Get(), TEXT( "Annotate" ) );
	const bool bUseSCC = bSyncSymbols || bAnnotate;

	if( bUseSCC )
	{
		InitSourceControl( false );
	}

	FWindowsPlatformStackWalkExt WindowsStackWalkExt( CrashInfo );

	const bool bReady = WindowsStackWalkExt.InitStackWalking();

	bool bHasAtLeastThreeValidFunctions = false;
	if( bReady && WindowsStackWalkExt.OpenDumpFile( InCrashDumpFilename ) )
	{
		if( CrashInfo.BuiltFromCL != FCrashInfo::INVALID_CHANGELIST )
		{
			// CrashInfo now contains a changelist to lookup a label for
			if( bSyncSymbols )
			{
				// Get the build version from the crash info.
				FCrashModuleInfo ExeFileVersion;
				WindowsStackWalkExt.GetExeFileVersionAndModuleList( ExeFileVersion );

				FindSymbolsAndBinariesStorage();

				const bool bSynced = SyncModules();
				// Without symbols we can't decode the provided minidump.
				if( !bSynced )
				{
					return 0;
				}
			}

			// Initialise the symbol options
			WindowsStackWalkExt.InitSymbols();

			// Set the symbol path based on the loaded modules
			WindowsStackWalkExt.SetSymbolPathsFromModules();

			// Get all the info we should ever need about the modules
			WindowsStackWalkExt.GetModuleInfoDetailed();

			// Get info about the system that created the minidump
			WindowsStackWalkExt.GetSystemInfo();

			// Get all the thread info
			WindowsStackWalkExt.GetThreadInfo();

			// Get exception info
			WindowsStackWalkExt.GetExceptionInfo();

			// Get the callstacks for each thread
			bHasAtLeastThreeValidFunctions = WindowsStackWalkExt.GetCallstacks() >= 3;

			// Sync the source file where the crash occurred
			if( CrashInfo.SourceFile.Len() > 0 )
			{
				if( bSyncSymbols && CrashInfo.BuiltFromCL > 0 )
				{
					UE_LOG( LogCrashDebugHelper, Log, TEXT( "Using CL %i to sync crash source file" ), CrashInfo.BuiltFromCL );
					SyncSourceFile();
				}

				// Try to annotate the file if requested
				bool bAnnotationSuccessful = false;
				if( bAnnotate )
				{
					bAnnotationSuccessful = AddAnnotatedSourceToReport();
				}

				// If annotation is not requested, or failed, add the standard source context
				if( !bAnnotationSuccessful )
				{
					AddSourceToReport();
				}
			}
		}
		else
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Invalid built from changelist" ) );
		}
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to open crash dump file: %s" ), *InCrashDumpFilename );
	}

	if( bUseSCC )
	{
		ShutdownSourceControl();
	}

	return bHasAtLeastThreeValidFunctions;
}

#include "HideWindowsPlatformTypes.h"
