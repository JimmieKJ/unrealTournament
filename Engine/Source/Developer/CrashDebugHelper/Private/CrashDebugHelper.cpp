// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "CrashDebugHelper.h"
#include "Database.h"
#include "ISourceControlModule.h"
#include "ISourceControlLabel.h"
#include "ISourceControlRevision.h"

#include "../../../../Launch/Resources/Version.h"

#ifndef MINIDUMPDIAGNOSTICS
	#define MINIDUMPDIAGNOSTICS	0
#endif

static const TCHAR* P4_DEPOT_PREFIX = TEXT( "//depot/" );

/** 
 * Global initialisation of this module
 */
bool ICrashDebugHelper::Init()
{
	bInitialized = true;

	// Look up the depot name
	// Try to use the command line param
	FString CmdLineBranchName;
	if ( FParse::Value(FCommandLine::Get(), TEXT("BranchName="), CmdLineBranchName) )
	{
		DepotName = FString::Printf( TEXT( "%s%s" ), P4_DEPOT_PREFIX, *CmdLineBranchName );
	}
	// Try to use what is configured in ini
	else if (GConfig->GetString(TEXT("Engine.CrashDebugHelper"), TEXT("DepotName"), DepotName, GEngineIni) == true)
	{
		// Hack to get around ini files treating '//' as an inlined comment
		DepotName.ReplaceInline(TEXT("\\"), TEXT("/"));
	}
	// Default to BRANCH_NAME
	else
	{
		DepotName = FString::Printf( TEXT( "%s%s" ), P4_DEPOT_PREFIX, TEXT( BRANCH_NAME ) );
	}

	CrashInfo.DepotName = DepotName;

	// Try to get the BuiltFromCL from command line to use this instead of attempting to locate the CL in the minidump
	FString CmdLineBuiltFromCL;
	int32 BuiltFromCL = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("BuiltFromCL="), CmdLineBuiltFromCL))
	{
		if ( !CmdLineBuiltFromCL.IsEmpty() )
		{
			BuiltFromCL = FCString::Atoi(*CmdLineBuiltFromCL);
		}
	}
	// Default to BUILT_FROM_CHANGELIST.
	else
	{
		BuiltFromCL = int32(BUILT_FROM_CHANGELIST);
	}

	CrashInfo.ChangelistBuiltFrom = BuiltFromCL;

	GConfig->GetString( TEXT( "Engine.CrashDebugHelper" ), TEXT( "SourceControlBuildLabelPattern" ), SourceControlBuildLabelPattern, GEngineIni );
	GConfig->GetString( TEXT( "Engine.CrashDebugHelper" ), TEXT( "ExecutablePathPattern" ), ExecutablePathPattern, GEngineIni );

	// Look up the local symbol store - fail if not found
	if (GConfig->GetString(TEXT("Engine.CrashDebugHelper"), TEXT("LocalSymbolStore"), LocalSymbolStore, GEngineIni) == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("Failed to get LocalSymbolStore from ini file... crash handling disabled"));
		bInitialized = false;
	}

	if( !GConfig->GetString( TEXT( "Engine.CrashDebugHelper" ), TEXT( "DepotRoot" ), DepotRoot, GEngineIni ) )
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to get DepotRoot from ini file... PDB Cache disabled" ) );
	}
	else
	{
		PDBCache.Init();
	}

	return bInitialized;
}

/** 
 * Initialise the source control interface, and ensure we have a valid connection
 */
bool ICrashDebugHelper::InitSourceControl(bool bShowLogin)
{
	// Ensure we are in a valid state to sync
	if (bInitialized == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("InitSourceControl: CrashDebugHelper is not initialized properly."));
		return false;
	}

	// Initialize the source control if it hasn't already been
	if( !ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable() )
	{
		// make sure our provider is set to Perforce
		ISourceControlModule::Get().SetProvider("Perforce");

		// Attempt to load in a source control module
		ISourceControlModule::Get().GetProvider().Init();
#if !MINIDUMPDIAGNOSTICS
		if ((ISourceControlModule::Get().GetProvider().IsAvailable() == false) || bShowLogin)
		{
			// Unable to connect? Prompt the user for login information
			ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless, EOnLoginWindowStartup::PreserveProvider);
		}
#endif
		// If it's still disabled, none was found, so exit
		if( !ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable() )
		{
			UE_LOG(LogCrashDebugHelper, Warning, TEXT("InitSourceControl: Source control unavailable or disabled."));
			return false;
		}
	}

	return true;
}

/** 
 * Shutdown the connection to source control
 */
void ICrashDebugHelper::ShutdownSourceControl()
{
	ISourceControlModule::Get().GetProvider().Close();
}

/** 
 * Sync the branch root relative file names to the requested label
 */
bool ICrashDebugHelper::SyncModules()
{
	if( CrashInfo.LabelName.Len() <= 0 )
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "SyncModules: Invalid Label parameter." ) );
		return false;
	}

	// Check source control
	if( !ISourceControlModule::Get().IsEnabled() )
	{
		return false;
	}

	const TCHAR* UESymbols = TEXT( "Rocket/Symbols/" );
	const bool bUseNetworkPathExecutables = !CrashInfo.NetworkPath.IsEmpty();

	// Get all labels associated with the crash info's label.
	// It should return one unique label because the crash info's label looks like this //depot/UE4-Releases/4.0/UE-CL-2047835 @see RetrieveBuildLabel
	// We can assume that the label name is unique.
	// 
	TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels( CrashInfo.LabelName );
	if( Labels.Num() == 1 )
	{
		TSharedRef<ISourceControlLabel> Label = Labels[0];
		TSet<FString> FilesToSync;

		// Use product version instead of label name to make a distinguish between chosen methods.
		const bool bContainsProductVersion = PDBCache.UsePDBCache() && PDBCache.ContainsPDBCacheEntry( CrashInfo.ProductVersion );
		const bool bContainsLabelName = PDBCache.UsePDBCache() && PDBCache.ContainsPDBCacheEntry( CrashInfo.LabelName );

		if( bContainsProductVersion )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "ProductVersion '%s' found in the PDB Cache, using it" ), *CrashInfo.ProductVersion );
			CrashInfo.PDBCacheEntry = PDBCache.FindAndTouchPDBCacheEntry( CrashInfo.ProductVersion );
			return true;
		}
		else if( bContainsLabelName )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Label '%s' found in the PDB Cache, using it" ), *CrashInfo.LabelName );
			CrashInfo.PDBCacheEntry = PDBCache.FindAndTouchPDBCacheEntry( CrashInfo.LabelName );
			return true;
		}
		else if( bUseNetworkPathExecutables )
		{			
			SCOPE_LOG_TIME_IN_SECONDS( TEXT( "SyncModulesAndNetwork" ), nullptr );

			// This is a bit hacky, probably will be valid until next build system change.
			const FString ProductNetworkPath = ExecutablePathPattern.Replace( TEXT( "%PRODUCT_VERSION%" ), *CrashInfo.ProductVersion );

			// Grab information about symbols.
			TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > PDBSourceControlRevisions;
			const FString PDBsPath = FString::Printf( TEXT( "%s/%s....pdb" ), *DepotName, UESymbols );
			Label->GetFileRevisions( PDBsPath, PDBSourceControlRevisions );

			TSet<FString> PDBPaths;
			for( const auto& PDBSrc : PDBSourceControlRevisions )
			{
				PDBPaths.Add( PDBSrc->GetFilename() );
			}

			// Now, sync symbols.
			for( const auto& PDBPath : PDBPaths )
			{
				if( Label->Sync( PDBPath ) )
				{
					UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Synced PDB '%s'" ), *PDBPath );
				}
			}

			// Find all the executables in the product network path.
			TArray<FString> NetworkExecutables;
			IFileManager::Get().FindFilesRecursive( NetworkExecutables, *ProductNetworkPath, TEXT( "*.dll" ), true, false, false );
			IFileManager::Get().FindFilesRecursive( NetworkExecutables, *ProductNetworkPath, TEXT( "*.exe" ), true, false, false );

			// From=Full pathname
			// To=Relative pathname
			TMap<FString, FString> FilesToBeCached;

			// If a symbol matches an executable, add the pair to the list of files that should be cached.
			for( const auto& NetworkExecutableFullpath : NetworkExecutables )
			{
				for( const auto& PDBPath : PDBPaths )
				{
					const FString PDBRelativePath = PDBPath.Replace( *DepotName, TEXT( "" ) ).Replace( UESymbols, TEXT( "" ) );
					const FString PDBFullpath = DepotRoot / PDBPath.Replace( P4_DEPOT_PREFIX, TEXT( "" ) );

					const FString PDBMatch = PDBRelativePath.Replace( TEXT( "pdb" ), TEXT( "" ) );
					const FString NetworkRelativePath = NetworkExecutableFullpath.Replace( *ProductNetworkPath, TEXT( "" ) );
					const bool bMatch = NetworkExecutableFullpath.Contains( PDBMatch );
					if( bMatch )
					{
						// From -> Where ?
						FilesToBeCached.Add( NetworkExecutableFullpath, NetworkRelativePath );
						FilesToBeCached.Add( PDBFullpath, PDBRelativePath );
						break;
					}
				}
			}

			if( PDBCache.UsePDBCache() )
			{
				// Initialize and add a new PDB Cache entry to the database.
				CrashInfo.PDBCacheEntry = PDBCache.CreateAndAddPDBCacheEntryMixed( CrashInfo.ProductVersion, FilesToBeCached );
			}
		}
		else
		{
			TArray<FString> FilesToBeCached;
			
			//@TODO: MAC: Excluding labels for Mac since we are only syncing windows binaries here...
			if( Label->GetName().Contains( TEXT( "Mac" ) ) )
			{
				UE_LOG( LogCrashDebugHelper, Log, TEXT( " Skipping Mac label '%s' when syncing modules." ), *Label->GetName() );
			}
			else
			{
				// Sync all the dll, exes, and related symbol files
				UE_LOG( LogCrashDebugHelper, Log, TEXT( " Syncing modules with label '%s'." ), *Label->GetName() );

				SCOPE_LOG_TIME_IN_SECONDS( TEXT( "SyncModules" ), nullptr );

				// Grab all dll and pdb files for the specified label.
				TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > DLLSourceControlRevisions;
				const FString DLLsPath = FString::Printf( TEXT( "%s/....dll" ), *DepotName );
				Label->GetFileRevisions( DLLsPath, DLLSourceControlRevisions );

				TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > EXESourceControlRevisions;
				const FString EXEsPath = FString::Printf( TEXT( "%s/....exe" ), *DepotName );
				Label->GetFileRevisions( EXEsPath, EXESourceControlRevisions );

				TArray< TSharedRef<class ISourceControlRevision, ESPMode::ThreadSafe> > PDBSourceControlRevisions;
				const FString PDBsPath = FString::Printf( TEXT( "%s/....pdb" ), *DepotName );
				Label->GetFileRevisions( PDBsPath, PDBSourceControlRevisions );

				TSet<FString> ModulesPaths;
				for( const auto& DLLSrc : DLLSourceControlRevisions )
				{
					ModulesPaths.Add( DLLSrc->GetFilename().Replace( *DepotName, TEXT( "" ) ) );
				}
				for( const auto& EXESrc : EXESourceControlRevisions )
				{
					ModulesPaths.Add( EXESrc->GetFilename().Replace( *DepotName, TEXT( "" ) ) );
				}

				TSet<FString> PDBPaths;
				for( const auto& PDBSrc : PDBSourceControlRevisions )
				{
					PDBPaths.Add( PDBSrc->GetFilename().Replace( *DepotName, TEXT( "" ) ) );
				}

				// Iterate through all module and see if we have dll and pdb associated with the module, if so add it to the files to sync.
				for( const auto& ModuleName : CrashInfo.ModuleNames )
				{
					const FString ModuleNamePDB = ModuleName.Replace( TEXT( ".dll" ), TEXT( ".pdb" ) ).Replace( TEXT( ".exe" ), TEXT( ".pdb" ) );

					for( const auto& ModulePath : ModulesPaths )
					{
						const bool bContainsModule = ModulePath.Contains( ModuleName );
						if( bContainsModule )
						{
							FilesToSync.Add( ModulePath );
						}
					}

					for( const auto& PDBPath : PDBPaths )
					{
						const bool bContainsPDB = PDBPath.Contains( ModuleNamePDB );
						if( bContainsPDB )
						{
							FilesToSync.Add( PDBPath );
						}
					}
				}

				// Now, sync all files.
				for( const auto& Filename : FilesToSync )
				{
					const FString DepotPath = DepotName + Filename;
					if( Label->Sync( DepotPath ) )
					{
						UE_LOG( LogCrashDebugHelper, Warning, TEXT( " ... synced binary '%s'." ), *DepotPath );
					}
					FilesToBeCached.Add( DepotPath );
				}
			}

			if( PDBCache.UsePDBCache() )
			{
				// Initialize and add a new PDB Cache entry to the database.
				CrashInfo.PDBCacheEntry = PDBCache.CreateAndAddPDBCacheEntry( CrashInfo.LabelName, DepotRoot, DepotName, FilesToBeCached );
			}
		}
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Error, TEXT( "Could not find label '%s'."), *CrashInfo.LabelName );
	}

	return true;
}

/** 
 * Sync a single source file to the requested label
 */
bool ICrashDebugHelper::SyncSourceFile()
{
	if (CrashInfo.LabelName.Len() <= 0)
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "SyncSourceFile: Invalid Label parameter." ) );
		return false;
	}

	// Check source control
	if( !ISourceControlModule::Get().IsEnabled() )
	{
		return false;
	}

	// Sync all the dll, exes, and related symbol files
	FString DepotPath = DepotName / CrashInfo.SourceFile;
	TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels( CrashInfo.LabelName );
	if(Labels.Num() > 0)
	{
		TSharedRef<ISourceControlLabel> Label = Labels[0];
		if( Label->Sync( DepotPath ) )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( " ... synced source file '%s'."), *DepotPath );
		}
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Error, TEXT( "Could not find label '%s'."), *CrashInfo.LabelName );
	}

	return true;
}

/**
 *	Load the given ANSI text file to an array of strings - one FString per line of the file.
 *	Intended for use in simple text parsing actions
 *
 *	@param	InFilename			The text file to read, full path
 *	@param	OutStrings			The array of FStrings to fill in
 *
 *	@return	bool				true if successful, false if not
 */
bool ICrashDebugHelper::ReadSourceFile( const TCHAR* InFilename, TArray<FString>& OutStrings )
{
	FString Line;
	if( FFileHelper::LoadFileToString( Line, InFilename ) )
	{
		Line = Line.Replace( TEXT( "\r" ), TEXT( "" ) );
		Line.ParseIntoArray( &OutStrings, TEXT( "\n" ), false );
		
		return true;
	}
	else
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to open source file %s" ), InFilename );
		return false;
	}
}

/** 
 * Add adjacent lines of the source file the crash occurred in the crash report
 */
void ICrashDebugHelper::AddSourceToReport()
{
	if( CrashInfo.SourceFile.Len() > 0 && CrashInfo.SourceLineNumber != 0 )
	{
		TArray<FString> Lines;
		FString FullPath = FString( TEXT( "../../../" ) ) + CrashInfo.SourceFile;
		ReadSourceFile( *FullPath, Lines );

		uint64 MinLine = FMath::Clamp( CrashInfo.SourceLineNumber - 15, ( uint64 )1, ( uint64 )Lines.Num() );
		uint64 MaxLine = FMath::Clamp( CrashInfo.SourceLineNumber + 15, ( uint64 )1, ( uint64 )Lines.Num() );

		for( int32 Line = MinLine; Line < MaxLine; Line++ )
		{
			if( Line == CrashInfo.SourceLineNumber - 1 )
			{
				CrashInfo.SourceContext.Add( FString( TEXT( "*****" ) ) + Lines[Line] );
			}
			else
			{
				CrashInfo.SourceContext.Add( FString( TEXT( "     " ) ) + Lines[Line] );
			}
		}
	}
}

/** 
 * Add source control annotated adjacent lines of the source file the crash occurred in the crash report
 */
bool ICrashDebugHelper::AddAnnotatedSourceToReport()
{
	// Make sure we have a source file to interrogate
	if( CrashInfo.SourceFile.Len() > 0 && CrashInfo.SourceLineNumber != 0 )
	{
		// Check source control
		if( !ISourceControlModule::Get().IsEnabled() )
		{
			return false;
		}

		// Ask source control to annotate the file for us
		FString DepotPath = DepotName / CrashInfo.SourceFile;

		TArray<FAnnotationLine> Lines;
		SourceControlHelpers::AnnotateFile( ISourceControlModule::Get().GetProvider(), CrashInfo.LabelName, DepotPath, Lines );

		uint64 MinLine = FMath::Clamp( CrashInfo.SourceLineNumber - 15, ( uint64 )1, ( uint64 )Lines.Num() );
		uint64 MaxLine = FMath::Clamp( CrashInfo.SourceLineNumber + 15, ( uint64 )1, ( uint64 )Lines.Num() );

		// Display a source context in the report, and decorate each line with the last editor of the line
		for( int32 Line = MinLine; Line < MaxLine; Line++ )
		{			
			if( Line == CrashInfo.SourceLineNumber )
			{
				CrashInfo.SourceContext.Add( FString::Printf( TEXT( "*****%20s: %s" ), *Lines[Line].UserName, *Lines[Line].Line ) );
			}
			else
			{
				CrashInfo.SourceContext.Add( FString::Printf( TEXT( "     %20s: %s" ), *Lines[Line].UserName, *Lines[Line].Line ) );
			}
		}
		return true;
	}

	return false;
}

/**
 * Add a line to the report
 */
void FCrashInfo::Log( FString Line )
{
	UE_LOG( LogCrashDebugHelper, Warning, TEXT("%s"), *Line );
	Report += Line + LINE_TERMINATOR;
}

/** 
 * Convert the processor architecture to a human readable string
 */
const TCHAR* FCrashInfo::GetProcessorArchitecture( EProcessorArchitecture PA )
{
	switch( PA )
	{
	case PA_X86:
		return TEXT( "x86" );
	case PA_X64:
		return TEXT( "x64" );
	case PA_ARM:
		return TEXT( "ARM" );
	}

	return TEXT( "Unknown" );
}

/** 
 * Calculate the byte size of a UTF-8 string
 */
int64 FCrashInfo::StringSize( const ANSICHAR* Line )
{
	int64 Size = 0;
	if( Line != NULL )
	{
		while( *Line++ != 0 )
		{
			Size++;
		}
	}
	return Size;
}

/** 
 * Write a line of UTF-8 to a file
 */
void FCrashInfo::WriteLine( FArchive* ReportFile, const ANSICHAR* Line )
{
	if( Line != NULL )
	{
		int64 StringBytes = StringSize( Line );
		ReportFile->Serialize( ( void* )Line, StringBytes );
	}

	ReportFile->Serialize( TCHAR_TO_UTF8( LINE_TERMINATOR ), FCStringWide::Strlen(LINE_TERMINATOR) );
}

/** 
 * Write all the data mined from the minidump to a text file
 */
void FCrashInfo::GenerateReport( const FString& DiagnosticsPath )
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter( *DiagnosticsPath );
	if( ReportFile != NULL )
	{
		FString Line;

		WriteLine( ReportFile, TCHAR_TO_UTF8( TEXT( "Generating report for minidump" ) ) );
		WriteLine( ReportFile );

		if ( ProductVersion.Len() > 0 )
		{
			Line = FString::Printf( TEXT( "Application version %s" ), *ProductVersion );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		else if( Modules.Num() > 0 )
		{
			Line = FString::Printf( TEXT( "Application version %d.%d.%d" ), Modules[0].Major, Modules[0].Minor, Modules[0].Patch );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}

		Line = FString::Printf( TEXT( " ... built from changelist %d" ), ChangelistBuiltFrom );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		if( LabelName.Len() > 0 )
		{
			Line = FString::Printf( TEXT( " ... based on label %s" ), *LabelName );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "OS version %d.%d.%d.%d" ), SystemInfo.OSMajor, SystemInfo.OSMinor, SystemInfo.OSBuild, SystemInfo.OSRevision );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		Line = FString::Printf( TEXT( "Running %d %s processors" ), SystemInfo.ProcessorCount, GetProcessorArchitecture( SystemInfo.ProcessorArchitecture ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		Line = FString::Printf( TEXT( "Exception was \"%s\"" ), *Exception.ExceptionString );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "Source context from \"%s\"" ), *SourceFile );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "<SOURCE START>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		for( int32 LineIndex = 0; LineIndex < SourceContext.Num(); LineIndex++ )
		{
			Line = FString::Printf( TEXT( "%s" ), *SourceContext[LineIndex] );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}
		Line = FString::Printf( TEXT( "<SOURCE END>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "<CALLSTACK START>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		for( int32 StackIndex = 0; StackIndex < Exception.CallStackString.Num(); StackIndex++ )
		{
			Line = FString::Printf( TEXT( "%s" ), *Exception.CallStackString[StackIndex] );
			WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		}

		Line = FString::Printf( TEXT( "<CALLSTACK END>" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );
		WriteLine( ReportFile );

		Line = FString::Printf( TEXT( "%d loaded modules" ), Modules.Num() );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line ) );

		for( int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++ )
		{
			FCrashModuleInfo& Module = Modules[ModuleIndex];

			FString ModuleDirectory = FPaths::GetPath(Module.Name);
			FString ModuleName = FPaths::GetBaseFilename( Module.Name, true ) + FPaths::GetExtension( Module.Name, true );

			FString ModuleDetail = FString::Printf( TEXT( "%40s" ), *ModuleName );
			FString Version = FString::Printf( TEXT( " (%d.%d.%d.%d)" ), Module.Major, Module.Minor, Module.Patch, Module.Revision );
			ModuleDetail += FString::Printf( TEXT( " %22s" ), *Version );
			ModuleDetail += FString::Printf( TEXT( " 0x%016x 0x%08x" ), Module.BaseOfImage, Module.SizeOfImage );
			ModuleDetail += FString::Printf( TEXT( " %s" ), *ModuleDirectory );

			WriteLine( ReportFile, TCHAR_TO_UTF8( *ModuleDetail ) );
		}

		WriteLine( ReportFile );

		// Write out the processor debugging log
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Report ) );

		Line = FString::Printf( TEXT( "Report end!" ) );
		WriteLine( ReportFile, TCHAR_TO_UTF8( *Line )  );

		ReportFile->Close();
		delete ReportFile;
	}
}

bool ICrashDebugHelper::SyncRequiredFilesForDebuggingFromLabel(const FString& InLabel, const FString& InPlatform)
{
	// Ensure we are in a valid state to sync
	if (bInitialized == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncRequiredFilesForDebuggingFromLabel: CrashDebugHelper is not initialized properly."));
		return false;
	}

	if (InLabel.Len() <= 0)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncRequiredFilesForDebuggingFromLabel: Invalid Label parameter."));
		return false;
	}

	if (InPlatform.Len() <= 0)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncRequiredFilesForDebuggingFromLabel: Invalid Platform parameter."));
		return false;
	}

	bool bSyncFiles = true;

	// We have a valid label... 
	// This command will get the list of all Win64 pdb files under engine at the given label
	//     p4 files //depot/UE4/Engine/Binaries/Win64/...pdb...@UE4_[2012-10-24_04.00]
	// This command will get the list of all Win64 pdb files under game folders at the given label
	//     p4 files //depot/UE4/...Game/Binaries/Win64/...pdb...@UE4_[2012-10-24_04.00]
	FString EngineRoot = FString::Printf(TEXT("%s/Engine/Binaries/%s/"), *DepotName, *InPlatform);
	FString EnginePdbFiles = EngineRoot + TEXT("...pdb...");
	FString EngineExeFiles = EngineRoot + TEXT("...exe...");
	FString EngineDllFiles = EngineRoot + TEXT("...dll...");

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	TSharedPtr<ISourceControlLabel> Label = SourceControlProvider.GetLabel(InLabel);
	if(Label.IsValid())
	{
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > EnginePdbList;
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > EngineExeList;
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > EngineDllList;

		bool bGotEngineFiles = true;
		bGotEngineFiles |= Label->GetFileRevisions(EnginePdbFiles, EnginePdbList);
		bGotEngineFiles |= Label->GetFileRevisions(EngineExeFiles, EngineExeList);
		bGotEngineFiles |= Label->GetFileRevisions(EngineDllFiles, EngineDllList);

		FString GameRoot = FString::Printf(TEXT("%s/...Game/Binaries/%s/"), *DepotName, *InPlatform);
		FString GamePdbFiles = GameRoot + TEXT("...pdb...");
		FString GameExeFiles = GameRoot + TEXT("...exe...");
		FString GameDllFiles = GameRoot + TEXT("...dll...");
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > GamePdbList;
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > GameExeList;
		TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > GameDllList;

		bool bGotGameFiles = true;
		bGotGameFiles |= Label->GetFileRevisions(GamePdbFiles, GamePdbList);
		bGotGameFiles |= Label->GetFileRevisions(GameExeFiles, GameExeList);
		bGotGameFiles |= Label->GetFileRevisions(GameDllFiles, GameDllList);

		if (bGotEngineFiles == true)
		{
			TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> > CopyFileList;
			CopyFileList += EnginePdbList;
			CopyFileList += EngineExeList;
			CopyFileList += EngineDllList;
			CopyFileList += GamePdbList;
			CopyFileList += GameExeList;
			CopyFileList += GameDllList;

			int32 CopyCount = 0;

			// Copy all the files retrieved
			for (int32 FileIdx = 0; FileIdx < CopyFileList.Num(); FileIdx++)
			{
				TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> Revision = CopyFileList[FileIdx];

				// Copy the files to a flat directory.
				// This will have problems if there are any files named the same!!!!
				FString LocalStoreFolder = LocalSymbolStore / InPlatform;
				FString CopyFilename = LocalStoreFolder / FPaths::GetCleanFilename(Revision->GetFilename());

//				UE_LOG(LogCrashDebugHelper, Display, TEXT("Copying engine file: %s to %s"), *Filename, *CopyFilename);

				if (Revision->Get(CopyFilename) == true)
				{
					CopyCount++;
				}
			}

			//@todo. Should we verify EVERY file was copied?
			return (CopyCount > 0);
		}
	}
	else
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncRequiredFiles: Invalid label specified: %s"), *InLabel);
	}

	return false;
}

bool ICrashDebugHelper::SyncRequiredFilesForDebuggingFromChangelist(int32 InChangelistNumber, const FString& InPlatform)
{
	//@todo. Allow for syncing a changelist directly?
	// Not really useful as the source indexed pdbs will be tied to labeled builds.
	// For now, we will not support this

	const FString BuildLabel = RetrieveBuildLabel(InChangelistNumber);
	if (BuildLabel.Len() > 0)
	{
		return SyncRequiredFilesForDebuggingFromLabel(BuildLabel, InPlatform);
	}

	UE_LOG(LogCrashDebugHelper, Warning, 
		TEXT("SyncRequiredFilesForDebuggingFromChangelist: Failed to find label for changelist %d"), InChangelistNumber);
	return false;
}

/**
 *	Retrieve the build label for the given engine version or changelist number.
 *
 *	@param	InChangelistNumber	The changelist number to retrieve the label for
 *	@return	FString				The label if successful, empty if it wasn't found or another error
 */
FString ICrashDebugHelper::RetrieveBuildLabel(int32 InChangelistNumber)
{
	FString FoundLabelString;

	if( InChangelistNumber < 0 )
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("RetrieveBuildLabel: Invalid parameters."));
		return FoundLabelString;
	}

	// Try to find the label directly in source control by using the pattern supplied via ini
	if ( FoundLabelString.IsEmpty() && InChangelistNumber >= 0 && !SourceControlBuildLabelPattern.IsEmpty() )
	{
		const FString ChangelistString = FString::Printf(TEXT("%d"), InChangelistNumber);
		const FString TestLabel = SourceControlBuildLabelPattern.Replace(TEXT("%CHANGELISTNUMBER%"), *ChangelistString, ESearchCase::CaseSensitive );
		TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels( TestLabel );
		if ( Labels.Num() > 0 )
		{
			const int32 LabelIndex = 0;
			// If we found more than one label, warn about it and just use the first one
			if ( Labels.Num() > 1 )
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("RetrieveBuildLabel: More than one build label found with pattern %s - Using label %s"), *TestLabel, *Labels[LabelIndex]->GetName());
			}

			FoundLabelString = Labels[LabelIndex]->GetName();
			UE_LOG(LogCrashDebugHelper, Log, TEXT("RetrieveBuildLabel: Found label %s matching pattern %s in source control."), *FoundLabelString, *TestLabel);
		}
	}

	return FoundLabelString;
}

void ICrashDebugHelper::RetrieveBuildLabelAndNetworkPath( int32 InChangelistNumber )
{
	if( InChangelistNumber < 0 )
	{
		UE_LOG( LogCrashDebugHelper, Warning, TEXT( "RetrieveBuildLabelAndNetworkPath: Invalid parameters." ) );
		CrashInfo.LabelName.Empty();
		CrashInfo.NetworkPath.Empty();
	}

	// Try to find the label directly in source control by using the pattern supplied via ini.
	if( InChangelistNumber >= 0 && !SourceControlBuildLabelPattern.IsEmpty() )
	{
		const FString ChangelistString = FString::Printf( TEXT( "%d" ), InChangelistNumber );
		const FString TestLabelWithCL = SourceControlBuildLabelPattern.Replace( TEXT( "%CHANGELISTNUMBER%" ), *ChangelistString, ESearchCase::CaseSensitive );
		TArray< TSharedRef<ISourceControlLabel> > Labels = ISourceControlModule::Get().GetProvider().GetLabels( TestLabelWithCL );
		if( Labels.Num() > 0 )
		{
			const int32 LabelIndex = 0;
			// If we found more than one label, warn about it and just use the first one
			if( Labels.Num() > 1 )
			{
				UE_LOG( LogCrashDebugHelper, Warning, TEXT( "RetrieveBuildLabelAndNetworkPath: More than one build label found with pattern '%s' - Using label '%s'" ), *TestLabelWithCL, *Labels[LabelIndex]->GetName() );
			}

			CrashInfo.LabelName = Labels[LabelIndex]->GetName();
			UE_LOG( LogCrashDebugHelper, Log, TEXT( "RetrieveBuildLabelAndNetworkPath: Found label '%s' matching pattern '%s' in source control." ), *CrashInfo.LabelName, *TestLabelWithCL );
		}
	}
	else
	{
		CrashInfo.LabelName.Empty();
	}

	// Try to find the network path by using the pattern supplied via ini.
	// If this step successes, we will grab the executable from the network path instead of P4.
	// At this moment we only care about UE4 releases.
	const bool bCanUseNetworkPath = DepotName.Contains( TEXT( "UE4-Releases" ) );
	if( bCanUseNetworkPath && InChangelistNumber >= 0 && !ExecutablePathPattern.IsEmpty() )
	{
		const FString TestNetworkPath = ExecutablePathPattern.Replace( TEXT( "%PRODUCT_VERSION%" ), *CrashInfo.ProductVersion );
		const bool bIsValidPath = IFileManager::Get().DirectoryExists( *TestNetworkPath );

		if( bIsValidPath )
		{
			CrashInfo.NetworkPath = TestNetworkPath;
			UE_LOG( LogCrashDebugHelper, Log, TEXT( "RetrieveBuildLabelAndNetworkPath: Found network path '%s'." ), *CrashInfo.NetworkPath );
		}
		else
		{
			CrashInfo.NetworkPath.Empty();
			UE_LOG( LogCrashDebugHelper, Log, TEXT( "RetrieveBuildLabelAndNetworkPath: Network path '%s' not found." ), *TestNetworkPath );
		}
	}
	else
	{
		CrashInfo.NetworkPath.Empty();
	}
}

/*-----------------------------------------------------------------------------
	PDB Cache implementation
-----------------------------------------------------------------------------*/

const TCHAR* FPDBCache::PDBTimeStampFileNoMeta = TEXT( "PDBTimeStamp.txt" );
const TCHAR* FPDBCache::PDBTimeStampFile = TEXT( "PDBTimeStamp.bin" );

void FPDBCache::Init()
{
	// PDB Cache
	// Default configuration
	//PDBCachePath=F:/CrashReportPDBCache/
	//DepotRoot=F:/depot
	//DaysToDeleteUnusedFilesFromPDBCache=7
	//PDBCacheSizeGB=128
	//MinDiskFreeSpaceGB=256

	// Can be enabled only through the command line.
	FParse::Bool( FCommandLine::Get(), TEXT( "bUsePDBCache=" ), bUsePDBCache );

	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "bUsePDBCache is %s" ), bUsePDBCache ? TEXT( "enabled" ) : TEXT( "disabled" ) );

	// Get the rest of the PDB cache configuration.
	if( bUsePDBCache )
	{
		if( !GConfig->GetString( TEXT( "Engine.CrashDebugHelper" ), TEXT( "PDBCachePath" ), PDBCachePath, GEngineIni ) )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to get PDBCachePath from ini file... PDB Cache disabled" ) );
			bUsePDBCache = false;
		}
	}

	if( bUsePDBCache )
	{
		if( !GConfig->GetInt( TEXT( "Engine.CrashDebugHelper" ), TEXT( "PDBCacheSizeGB" ), PDBCacheSizeGB, GEngineIni ) )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to get PDBCachePath from ini file... Using default value" ) );
		}

		if( !GConfig->GetInt( TEXT( "Engine.CrashDebugHelper" ), TEXT( "MinDiskFreeSpaceGB" ), MinDiskFreeSpaceGB, GEngineIni ) )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to get MinDiskFreeSpaceGB from ini file... Using default value" ) );
		}

		if( !GConfig->GetInt( TEXT( "Engine.CrashDebugHelper" ), TEXT( "DaysToDeleteUnusedFilesFromPDBCache" ), DaysToDeleteUnusedFilesFromPDBCache, GEngineIni ) )
		{
			UE_LOG( LogCrashDebugHelper, Warning, TEXT( "Failed to get DaysToDeleteUnusedFilesFromPDBCache from ini file... Using default value" ) );
		}

		InitializePDBCache();
		CleanPDBCache( DaysToDeleteUnusedFilesFromPDBCache );

		// Verify that we have enough space to enable the PDB Cache.
		uint64 TotalNumberOfBytes = 0;
		uint64 NumberOfFreeBytes = 0;
		FPlatformMisc::GetDiskTotalAndFreeSpace( PDBCachePath, TotalNumberOfBytes, NumberOfFreeBytes );

		const int32 TotalDiscSpaceGB = int32( TotalNumberOfBytes >> 30 );
		const int32 DiskFreeSpaceGB = int32( NumberOfFreeBytes >> 30 );

		if( DiskFreeSpaceGB < MinDiskFreeSpaceGB )
		{
			// There is not enough free space, calculate the current PDB cache usage and try removing the old data.
			const int32 CurrentPDBCacheSizeGB = GetPDBCacheSizeGB();
			const int32 DiskFreeSpaceAfterCleanGB = DiskFreeSpaceGB + CurrentPDBCacheSizeGB;
			if( DiskFreeSpaceAfterCleanGB < MinDiskFreeSpaceGB )
			{
				UE_LOG( LogCrashDebugHelper, Error, TEXT( "There is not enough free space. PDB Cache disabled." ) );
				UE_LOG( LogCrashDebugHelper, Error, TEXT( "Current disk free space is %i GBs." ), DiskFreeSpaceGB );
				UE_LOG( LogCrashDebugHelper, Error, TEXT( "To enable the PDB Cache you need to free %i GB of space" ), MinDiskFreeSpaceGB - DiskFreeSpaceAfterCleanGB );
				bUsePDBCache = false;
				// Remove all data.
				CleanPDBCache( 0 );
			}
			else
			{
				// Clean the PDB cache until we get enough free space.
				CleanPDBCache( DaysToDeleteUnusedFilesFromPDBCache, MinDiskFreeSpaceGB - DiskFreeSpaceGB );
			}
		}
	}

	if( bUsePDBCache )
	{
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDBCachePath=%s" ), *PDBCachePath );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDBCacheSizeGB=%i" ), PDBCacheSizeGB );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "MinDiskFreeSpaceGB=%i" ), MinDiskFreeSpaceGB );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "DaysToDeleteUnusedFilesFromPDBCache=%i" ), DaysToDeleteUnusedFilesFromPDBCache );
	}
}


void FPDBCache::InitializePDBCache()
{
	const double StartTime = FPlatformTime::Seconds();
	IFileManager::Get().MakeDirectory( *PDBCachePath, true );

	TArray<FString> PDBCacheEntryDirectories;
	IFileManager::Get().FindFiles( PDBCacheEntryDirectories, *PDBCachePath, false, true );

	for( const auto& Directory : PDBCacheEntryDirectories )
	{
		FPDBCacheEntryRef Entry = ReadPDBCacheEntry( Directory );
		PDBCacheEntries.Add( Directory, Entry );
	}

	SortPDBCache();

	const double TotalTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDB Cache initialized in %.2f ms" ), TotalTime*1000.0f );
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "Found %i entries which occupy %i GBs" ), PDBCacheEntries.Num(), GetPDBCacheSizeGB() );
}

void FPDBCache::CleanPDBCache( int32 DaysToDelete, int32 NumberOfGBsToBeCleaned /*= 0 */ )
{
	// Not very efficient, but should do the trick.
	// Revisit it later.
	const double StartTime = FPlatformTime::Seconds();

	TSet<FString> EntriesToBeRemoved;

	// Find all outdated PDB Cache entries and mark them for removal.
	const double DaysToDeleteAsSeconds = FTimespan( DaysToDelete, 0, 0, 0 ).GetTotalSeconds();
	int32 NumGBsCleaned = 0;
	for( const auto& It : PDBCacheEntries )
	{
		const FPDBCacheEntryRef& Entry = It.Value;
		const FString EntryDirectory = PDBCachePath / Entry->Directory;
		const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

		const double EntryFileAge = IFileManager::Get().GetFileAgeSeconds( *EntryTimeStampFilename );
		if( EntryFileAge > DaysToDeleteAsSeconds )
		{
			EntriesToBeRemoved.Add( Entry->Directory );
			NumGBsCleaned += Entry->SizeGB;
		}
	}

	if( NumberOfGBsToBeCleaned > 0 && NumGBsCleaned < NumberOfGBsToBeCleaned )
	{
		// Do the second pass if we need to remove more PDB Cache entries due to the free disk space restriction.
		for( const auto& It : PDBCacheEntries )
		{
			const FPDBCacheEntryRef& Entry = It.Value;

			if( !EntriesToBeRemoved.Contains( Entry->Directory ) )
			{
				EntriesToBeRemoved.Add( Entry->Directory );
				NumGBsCleaned += Entry->SizeGB;

				if( NumGBsCleaned > NumberOfGBsToBeCleaned )
				{
					// Break the loop, we are done.
					break;
				}
			}
		}
	}

	// Remove all marked PDB Cache entries.
	for( const auto& EntryDirectory : EntriesToBeRemoved )
	{
		RemovePDBCacheEntry( EntryDirectory );
	}

	const double TotalTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDB Cache cleaned %i GBs in %.2f ms" ), NumGBsCleaned, TotalTime*1000.0f );
}

FPDBCacheEntryRef FPDBCache::CreateAndAddPDBCacheEntry( const FString& OriginalLabelName, const FString& DepotRoot, const FString& DepotName, const TArray<FString>& FilesToBeCached )
{
	const FString CleanedLabelName = EscapePath( OriginalLabelName );
	const FString EntryDirectory = PDBCachePath / CleanedLabelName;
	const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

	const FString LocalDepotDir = DepotRoot / DepotName.Replace( P4_DEPOT_PREFIX, TEXT( "" ) );

	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "PDB Cache entry '%s' is being copied from '%s', it will take some time" ), *CleanedLabelName, *OriginalLabelName );
	for( const auto& Filename : FilesToBeCached )
	{
		const FString SourceDirectoryWithSearch = Filename.Replace( *DepotName, *LocalDepotDir );

		TArray<FString> MatchedFiles;
		IFileManager::Get().FindFiles( MatchedFiles, *SourceDirectoryWithSearch, true, false );

		for( const auto& MatchedFilename : MatchedFiles )
		{
			const FString SrcFilename = FPaths::GetPath( SourceDirectoryWithSearch ) / MatchedFilename;
			const FString DestFilename = EntryDirectory / SrcFilename.Replace( *LocalDepotDir, TEXT("") );
			IFileManager::Get().Copy( *DestFilename, *SrcFilename );
		}
	}


	TArray<FString> CachedFiles;
	IFileManager::Get().FindFilesRecursive( CachedFiles, *EntryDirectory, TEXT( "*.*" ), true, false );

	// Calculate the size of this PDB Cache entry.
	int64 TotalSize = 0;
	for( const auto& Filename : CachedFiles )
	{
		const int64 FileSize = IFileManager::Get().FileSize( *Filename );
		TotalSize += FileSize;
	}

	// Round-up the size.
	int32 SizeGB = (int32)FMath::DivideAndRoundUp( TotalSize, (int64)NUM_BYTES_PER_GB );
	FPDBCacheEntryRef NewCacheEntry = MakeShareable( new FPDBCacheEntry( CachedFiles, CleanedLabelName, FDateTime::Now(), SizeGB ) );

	// Verify there is an entry timestamp file, write the size of a PDB cache to avoid time consuming FindFilesRecursive during initialization.
	TAutoPtr<FArchive> FileWriter( IFileManager::Get().CreateFileWriter( *EntryTimeStampFilename ) );
	UE_CLOG( !FileWriter, LogCrashDebugHelper, Fatal, TEXT( "Couldn't save the timestamp file to %s" ), *EntryTimeStampFilename );
	*FileWriter << *NewCacheEntry;
	
	PDBCacheEntries.Add( CleanedLabelName, NewCacheEntry );
	SortPDBCache();

	return NewCacheEntry;
}

FPDBCacheEntryRef FPDBCache::CreateAndAddPDBCacheEntryMixed( const FString& ProductVersion, const TMap<FString, FString>& FilesToBeCached )
{
	// Enable MDD to parse all minidumps regardless the branch, to fix the issue with the missing executables on the P4 due to the build system changes.
	const FString EntryDirectory = PDBCachePath / ProductVersion;
	const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;
 
	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "PDB Cache entry '%s' is being created from %i files, it will take some time" ), *ProductVersion, FilesToBeCached.Num() );
	for( const auto& It : FilesToBeCached )
	{
		const FString& SrcFilename = It.Key;
		const FString DestFilename = EntryDirectory / It.Value;
		IFileManager::Get().Copy( *DestFilename, *SrcFilename );
	}

	TArray<FString> CachedFiles;
	IFileManager::Get().FindFilesRecursive( CachedFiles, *EntryDirectory, TEXT( "*.*" ), true, false );
 
	// Calculate the size of this PDB Cache entry.
	int64 TotalSize = 0;
	for( const auto& Filename : CachedFiles )
	{
		const int64 FileSize = IFileManager::Get().FileSize( *Filename );
		TotalSize += FileSize;
	}
 
	// Round-up the size.
	int32 SizeGB = (int32)FMath::DivideAndRoundUp( TotalSize, (int64)NUM_BYTES_PER_GB );
	FPDBCacheEntryRef NewCacheEntry = MakeShareable( new FPDBCacheEntry( CachedFiles, ProductVersion, FDateTime::Now(), SizeGB ) );

	// Verify there is an entry timestamp file, write the size of a PDB cache to avoid time consuming FindFilesRecursive during initialization.
	TAutoPtr<FArchive> FileWriter( IFileManager::Get().CreateFileWriter( *EntryTimeStampFilename ) );
	UE_CLOG( !FileWriter, LogCrashDebugHelper, Fatal, TEXT( "Couldn't save the timestamp file to %s" ), *EntryTimeStampFilename );
	*FileWriter << *NewCacheEntry;
 
	PDBCacheEntries.Add( ProductVersion, NewCacheEntry );
	SortPDBCache();
	
	return NewCacheEntry;
}

FPDBCacheEntryRef FPDBCache::ReadPDBCacheEntry( const FString& Directory )
{
	const FString EntryDirectory = PDBCachePath / Directory;
	const FString EntryTimeStampFilenameNoMeta = EntryDirectory / PDBTimeStampFileNoMeta;
	const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

	// Verify there is an entry timestamp file.
	const FDateTime LastAccessTimeNoMeta = IFileManager::Get().GetTimeStamp( *EntryTimeStampFilenameNoMeta );
	const FDateTime LastAccessTime = IFileManager::Get().GetTimeStamp( *EntryTimeStampFilename );

	FPDBCacheEntryPtr NewEntry;

	if( LastAccessTime != FDateTime::MinValue() )
	{
		// Read the metadata
		TAutoPtr<FArchive> FileReader( IFileManager::Get().CreateFileReader( *EntryTimeStampFilename ) );
		NewEntry = MakeShareable( new FPDBCacheEntry( LastAccessTime ) );
		*FileReader << *NewEntry;
	}
	else if( LastAccessTimeNoMeta != FDateTime::MinValue() )
	{
		// Calculate the size of this PDB Cache entry and update to the new version.
		TArray<FString> PDBFiles;
		IFileManager::Get().FindFilesRecursive( PDBFiles, *EntryDirectory, TEXT( "*.*" ), true, false );

		// Calculate the size of this PDB Cache entry.
		int64 TotalSize = 0;
		for( const auto& Filename : PDBFiles )
		{
			const int64 FileSize = IFileManager::Get().FileSize( *Filename );
			TotalSize += FileSize;
		}

		// Round-up the size.
		const int32 SizeGB = (int32)FMath::DivideAndRoundUp( TotalSize, (int64)NUM_BYTES_PER_GB );
		NewEntry = MakeShareable( new FPDBCacheEntry( PDBFiles, Directory, LastAccessTimeNoMeta, SizeGB ) );

		// Remove the old file and save the metadata.
		TAutoPtr<FArchive> FileWriter( IFileManager::Get().CreateFileWriter( *EntryTimeStampFilename ) );
		*FileWriter << *NewEntry;

		IFileManager::Get().Delete( *EntryTimeStampFilenameNoMeta );
	}
	else
	{
		// Something wrong.
		check( 0 );
	}

	return NewEntry.ToSharedRef();
}

void FPDBCache::TouchPDBCacheEntry( const FString& Directory )
{
	const FString EntryDirectory = PDBCachePath / Directory;
	const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

	FPDBCacheEntryRef& Entry = PDBCacheEntries.FindChecked( Directory );
	Entry->SetLastAccessTimeToNow();

	const bool bResult = IFileManager::Get().SetTimeStamp( *EntryTimeStampFilename, Entry->LastAccessTime );
	SortPDBCache();
}

void FPDBCache::RemovePDBCacheEntry( const FString& Directory )
{
	const double StartTime = FPlatformTime::Seconds();

	const FString EntryDirectory = PDBCachePath / Directory;

	FPDBCacheEntryRef& Entry = PDBCacheEntries.FindChecked( Directory );
	IFileManager::Get().DeleteDirectory( *EntryDirectory, true, true );
	
	const double TotalTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "PDB Cache entry %s removed in %.2f ms, restored %i GBs" ), *Directory, TotalTime*1000.0f, Entry->SizeGB );

	PDBCacheEntries.Remove( Directory );
}

FPDBCacheEntryRef FPDBCache::FindAndTouchPDBCacheEntry( const FString& PathOrLabel )
{
	FPDBCacheEntryRef CacheEntry = PDBCacheEntries.FindChecked( EscapePath( PathOrLabel ) );
	TouchPDBCacheEntry( CacheEntry->Directory );
	return CacheEntry;
}
