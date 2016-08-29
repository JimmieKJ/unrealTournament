// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "CrashDebugHelperWindows.h"
#include "WindowsPlatformStackWalkExt.h"

#include "EngineVersion.h"
#include "ISourceControlModule.h"

#include "AllowWindowsPlatformTypes.h"
#include <DbgHelp.h>

bool FCrashDebugHelperWindows::CreateMinidumpDiagnosticReport( const FString& InCrashDumpFilename )
{
	const bool bSyncSymbols = FParse::Param( FCommandLine::Get(), TEXT( "SyncSymbols" ) );
	const bool bAnnotate = FParse::Param( FCommandLine::Get(), TEXT( "Annotate" ) );
	const bool bNoTrim = FParse::Param(FCommandLine::Get(), TEXT("NoTrimCallstack"));
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
		if (CrashInfo.BuiltFromCL != FCrashInfo::INVALID_CHANGELIST)
		{
			// Get the build version and modules paths.
			FCrashModuleInfo ExeFileVersion;
			WindowsStackWalkExt.GetExeFileVersionAndModuleList(ExeFileVersion);

			// Init Symbols
			if (CrashInfo.bMutexPDBCache && !CrashInfo.PDBCacheLockName.IsEmpty())
			{
				// Scoped lock
				FSystemWideCriticalSection PDBCacheLock(CrashInfo.PDBCacheLockName, FTimespan(0, 0, 3, 0, 0));
				if (PDBCacheLock.IsValid())
				{
					InitSymbols(WindowsStackWalkExt, bSyncSymbols);
				}
			}
			else
			{
				InitSymbols(WindowsStackWalkExt, bSyncSymbols);
			}

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
			bHasAtLeastThreeValidFunctions = WindowsStackWalkExt.GetCallstacks(!bNoTrim) >= 3;

			// Sync the source file where the crash occurred
			if( CrashInfo.SourceFile.Len() > 0 )
			{
				const bool bMutexSourceSync = FParse::Param(FCommandLine::Get(), TEXT("MutexSourceSync"));
				FString SourceSyncLockName;
				FParse::Value(FCommandLine::Get(), TEXT("SourceSyncLock="), SourceSyncLockName);

				if (bMutexSourceSync && !SourceSyncLockName.IsEmpty())
				{
					// Scoped lock
					const FTimespan GlobalLockWaitTimeout(0, 0, 0, 30, 0);
					FSystemWideCriticalSection SyncSourceLock(SourceSyncLockName, GlobalLockWaitTimeout);
					if (SyncSourceLock.IsValid())
					{
						SyncAndReadSourceFile(bSyncSymbols, bAnnotate, CrashInfo.BuiltFromCL);
					}	
				}
				else
				{
					SyncAndReadSourceFile(bSyncSymbols, bAnnotate, CrashInfo.BuiltFromCL);
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

bool FCrashDebugHelperWindows::InitSymbols(FWindowsPlatformStackWalkExt& WindowsStackWalkExt, bool bSyncSymbols)
{
	// CrashInfo now contains a changelist to lookup a label for
	if (bSyncSymbols)
	{
		FindSymbolsAndBinariesStorage();

		const bool bSynced = SyncModules();
		// Without symbols we can't decode the provided minidump.
		if (!bSynced)
		{
			return false;
		}
	}

	// Initialise the symbol options
	WindowsStackWalkExt.InitSymbols();

	// Set the symbol path based on the loaded modules
	WindowsStackWalkExt.SetSymbolPathsFromModules();

	return true;
}

void FCrashDebugHelperWindows::SyncAndReadSourceFile(bool bSyncSymbols, bool bAnnotate, int32 BuiltFromCL)
{
	if (bSyncSymbols && BuiltFromCL > 0)
	{
		UE_LOG(LogCrashDebugHelper, Log, TEXT("Using CL %i to sync crash source file"), BuiltFromCL);
		SyncSourceFile();
	}

	// Try to annotate the file if requested
	bool bAnnotationSuccessful = false;
	if (bAnnotate)
	{
		bAnnotationSuccessful = AddAnnotatedSourceToReport();
	}

	// If annotation is not requested, or failed, add the standard source context
	if (!bAnnotationSuccessful)
	{
		AddSourceToReport();
	}
}

#include "HideWindowsPlatformTypes.h"
