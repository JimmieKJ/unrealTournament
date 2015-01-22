// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "EngineVersion.h"
#include "Windows/WindowsPlatformStackWalkExt.h"
#include "ISourceControlModule.h"

#include "AllowWindowsPlatformTypes.h"

bool FCrashDebugHelperWindows::CreateMinidumpDiagnosticReport( const FString& InCrashDumpFilename )
{
	const bool bSyncSymbols = FParse::Param( FCommandLine::Get(), TEXT( "SyncSymbols" ) );
	const bool bAnnotate = FParse::Param( FCommandLine::Get(), TEXT( "Annotate" ) );
	const bool bSyncMicrosoftSymbols = FParse::Param( FCommandLine::Get(), TEXT( "SyncMicrosoftSymbols" ) );
	const bool bUseSCC = bSyncSymbols || bAnnotate || bSyncMicrosoftSymbols;

	if( bUseSCC )
	{
		InitSourceControl(false);
	}

	FWindowsPlatformStackWalkExt WindowsStackWalkExt( CrashInfo );

	const bool bReady = WindowsStackWalkExt.InitStackWalking();

	bool bAtLeastOneFunctionNameFoundInCallstack = false;
	if( bReady && WindowsStackWalkExt.OpenDumpFile( InCrashDumpFilename ) )
	{
		if( CrashInfo.ChangelistBuiltFrom != FCrashInfo::INVALID_CHANGELIST )
		{
			// Get the build version from the crash info.
			FCrashModuleInfo ExeFileVersion;
			WindowsStackWalkExt.GetExeFileVersionAndModuleList( ExeFileVersion );

			// @see AutomationTool.CommandUtils.EscapePath 
			const FString CleanedDepotName = CrashInfo.DepotName.Replace( TEXT( ":" ), TEXT( "" ) ).Replace( TEXT( "/" ), TEXT( "+" ) ).Replace( TEXT( "\\" ), TEXT( "+" ) ).Replace( TEXT( " " ), TEXT( "+" ) );
			const FEngineVersion EngineVersion( ExeFileVersion.Major, ExeFileVersion.Minor, ExeFileVersion.Patch, CrashInfo.ChangelistBuiltFrom, CleanedDepotName );
			CrashInfo.ProductVersion = EngineVersion.ToString();

			// CrashInfo now contains a changelist to lookup a label for
			if( bSyncSymbols )
			{
				RetrieveBuildLabelAndNetworkPath( CrashInfo.ChangelistBuiltFrom );
				SyncModules();
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
			bAtLeastOneFunctionNameFoundInCallstack = WindowsStackWalkExt.GetCallstacks();

			// Sync the source file where the crash occurred
			if( CrashInfo.SourceFile.Len() > 0 )
			{
				if( bSyncSymbols )
				{
					CrashInfo.Log( FString::Printf( TEXT( " ... using label '%s' to sync crash source file" ), *CrashInfo.LabelName ) );
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
			CrashInfo.Log( FString::Printf( TEXT( "CreateMinidumpDiagnosticReport: Failed to find executable image; possibly pathless module names?" ), *InCrashDumpFilename ) );
		}
	}
	else
	{
		CrashInfo.Log( FString::Printf( TEXT( "CreateMinidumpDiagnosticReport: Failed to open crash dump file: %s" ), *InCrashDumpFilename ) );
	}

	if( bUseSCC )
	{
		ShutdownSourceControl();
	}

	return bAtLeastOneFunctionNameFoundInCallstack;
}

#include "HideWindowsPlatformTypes.h"
