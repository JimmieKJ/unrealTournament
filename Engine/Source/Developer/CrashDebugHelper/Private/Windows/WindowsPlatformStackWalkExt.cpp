// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"

#include "ModuleManager.h"
#include "WindowsPlatformStackWalkExt.h"

#include "AllowWindowsPlatformTypes.h"
#include "dbgeng.h"
#include <DbgHelp.h>
#include "HideWindowsPlatformTypes.h"

#pragma comment( lib, "dbgeng.lib" )

static IDebugClient5* Client = NULL;
static IDebugControl4* Control = NULL;
static IDebugSymbols3* Symbol = NULL;
static IDebugAdvanced3* Advanced = NULL;

FWindowsPlatformStackWalkExt::FWindowsPlatformStackWalkExt( FCrashInfo& InCrashInfo ) 
	: CrashInfo( InCrashInfo )
{}

FWindowsPlatformStackWalkExt::~FWindowsPlatformStackWalkExt()
{
	ShutdownStackWalking();
}


/**
 * Initialise the COM interface to grab stacks
 */
bool FWindowsPlatformStackWalkExt::InitStackWalking()
{
	if (!FWindowsPlatformMisc::CoInitialize())
	{
		return false;
	}

	check( DebugCreate( __uuidof( IDebugClient5 ), ( void** )&Client ) == S_OK );

	check( Client->QueryInterface( __uuidof( IDebugControl4 ), ( void** )&Control ) == S_OK );
	check( Client->QueryInterface( __uuidof( IDebugSymbols3 ), ( void** )&Symbol ) == S_OK );
	check( Client->QueryInterface( __uuidof( IDebugAdvanced3 ), ( void** )&Advanced ) == S_OK );

	return true;
}

/**
 * Shutdown COM
 */
void FWindowsPlatformStackWalkExt::ShutdownStackWalking()
{
	Advanced->Release();
	Symbol->Release();
	Control->Release();
	
	Client->Release();

	FWindowsPlatformMisc::CoUninitialize();
}

/**
 * Set the options we want for symbol lookup
 */
void FWindowsPlatformStackWalkExt::InitSymbols()
{
	ULONG SymOpts = 0;

	// Load line information
	SymOpts |= SYMOPT_LOAD_LINES;
	SymOpts |= SYMOPT_OMAP_FIND_NEAREST;
	// Fail if a critical error is encountered
	SymOpts |= SYMOPT_FAIL_CRITICAL_ERRORS;
	// Always load immediately; no deferred loading
	SymOpts |= SYMOPT_DEFERRED_LOADS;
	// Require an exact symbol match
	SymOpts |= SYMOPT_EXACT_SYMBOLS;
	// This option allows for undecorated names to be handled by the symbol engine.
	SymOpts |= SYMOPT_UNDNAME;

	// Disable by default as it can be very spammy/slow.  Turn it on if you are debugging symbol look-up!
	SymOpts |= SYMOPT_DEBUG;

	Symbol->SetSymbolOptions( SymOpts );
}

/** 
 * Grab the branch relative path of the binary
 */
FString FWindowsPlatformStackWalkExt::ExtractRelativePath( const TCHAR* BaseName, TCHAR* FullName )
{
	FString FullPath = FString( FullName ).ToLower();
	FullPath = FullPath.Replace( TEXT( "\\" ), TEXT( "/" ) );

	TArray<FString> Components;
	int32 Count = FullPath.ParseIntoArray( &Components, TEXT( "/" ), true );
	FullPath = TEXT( "" );

	for( int32 Index = 0; Index < Count; Index++ )
	{
		if( Components[Index] == BaseName )
		{
			if( Index > 0 )
			{
				for( int32 Inner = Index - 1; Inner < Count; Inner++ )
				{
					FullPath += Components[Inner];
					if( Inner < Count - 1 )
					{
						FullPath += TEXT( "/" );
					}
				}
			}
			break;
		}
	}

	return FullPath;
}

void FWindowsPlatformStackWalkExt::GetExeFileVersionAndModuleList( FCrashModuleInfo& out_ExeFileVersion )
{
	// The the number of loaded modules
	ULONG LoadedModuleCount = 0;
	ULONG UnloadedModuleCount = 0;
	Symbol->GetNumberModules( &LoadedModuleCount, &UnloadedModuleCount );

	CrashInfo.Log( FString::Printf( TEXT( " ... found %d loaded, and %d unloaded modules." ), LoadedModuleCount, UnloadedModuleCount ) );

	// Find the relative names of all the modules so we know which files to sync
	int ExecutableIndex = -1;
	for( uint32 ModuleIndex = 0; ModuleIndex < LoadedModuleCount; ModuleIndex++ )
	{
		ULONG64 ModuleBase = 0;
		Symbol->GetModuleByIndex( ModuleIndex, &ModuleBase );

		// Get the full path of the module name
		TCHAR ModuleName[MAX_PATH] = {0};
		Symbol->GetModuleNameStringWide( DEBUG_MODNAME_IMAGE, ModuleIndex, ModuleBase, ModuleName, MAX_PATH, NULL );

		FString RelativeModuleName = ExtractRelativePath( TEXT( "binaries" ), ModuleName );
		if( RelativeModuleName.Len() > 0 )
		{
			CrashInfo.ModuleNames.Add( RelativeModuleName );
		}

		// Get the exe, which we extract the version number, so we know what label to sync to
		if( RelativeModuleName.EndsWith( TEXT( ".exe" ) ) )
		{
			ExecutableIndex = ModuleIndex;
		}
	}

	// Get the executable version info.
	if( ExecutableIndex > -1 )
	{
		VS_FIXEDFILEINFO VersionInfo = {0};
		Symbol->GetModuleVersionInformationWide( ExecutableIndex, 0, TEXT( "\\" ), &VersionInfo, sizeof( VS_FIXEDFILEINFO ), NULL );

		out_ExeFileVersion.Major = HIWORD( VersionInfo.dwProductVersionMS );
		out_ExeFileVersion.Minor = LOWORD( VersionInfo.dwProductVersionMS );
		out_ExeFileVersion.Patch = HIWORD( VersionInfo.dwProductVersionLS );
	}
	else
	{
		CrashInfo.Log( FString::Printf( TEXT( " ... unable to locate executable." ) ) );
	}
}

/**
 *	Optimized version used to grab the list of the PDB and binary files.
 *	Based on the FPackageDependencyTimestampVisitor.
 */
class FSymbolAndExecutableFinder : public IPlatformFile::FDirectoryVisitor
{
private:
	/** The file interface to use for any file operations. */
	IPlatformFile& FileInterface;

	/**
	 *	A list of file types that should be included in the file paths.
	 *	An extension with a dot.
	 */
	TArray<FString> FiletypesToInclude;

	/** A list of directories that we should not traverse into. */
	TArray<FString> DirectoriesToIgnore;

	/** Relative paths to local files. */
	TArray<FString> FilePaths;

public:

	FSymbolAndExecutableFinder( IPlatformFile& InFileInterface, const TArray<FString>& InFiletypesToInclude, const TArray<FString>& InDirectoriesToIgnore )
		: FileInterface( InFileInterface )
		, FiletypesToInclude( InFiletypesToInclude )
		, DirectoriesToIgnore( InDirectoriesToIgnore )
	{}

	virtual bool Visit( const TCHAR* FilenameOrDirectory, bool bIsDirectory )
	{
		// Make sure all paths are "standardized" so the other end can match up with it's own standardized paths
		FString RelativeFilename = FilenameOrDirectory;
		FPaths::MakeStandardFilename( RelativeFilename );

		// Grab the file.
		if( !bIsDirectory )
		{
			if( FiletypesToInclude.Num() > 0 )
			{
				// If it is a file that matches the filetypes, add it.
				for( const auto& FileExtension : FiletypesToInclude )
				{
					if( RelativeFilename.Contains( FileExtension ) )
					{
						new(FilePaths) FString( RelativeFilename );
						break;
					}
				}
			}
			else
			{
				new(FilePaths) FString( RelativeFilename );
			}
		}

		// Iterate over directories we care about.
		if( bIsDirectory )
		{
			bool bShouldRecurse = true;
			// Look in all the ignore directories looking for a match.
			for( const auto& Directory : DirectoriesToIgnore )
			{
				if( RelativeFilename.Contains( Directory ) )
				{
					bShouldRecurse = false;
					break;
				}
			}

			// Recurse if we should.
			if( bShouldRecurse )
			{
				FileInterface.IterateDirectory( FilenameOrDirectory, *this );
			}
		}

		return true;
	}

	
	/**
	 * @return a reference to the results.
	 */
	const TArray<FString>& GetFilePaths() const
	{
		return FilePaths;
	}
};

/** 
 * Set the symbol paths based on the module paths
 */
void FWindowsPlatformStackWalkExt::SetSymbolPathsFromModules()
{
	const bool bUseCachedData = CrashInfo.PDBCacheEntry.IsValid();
	FString CombinedPath = TEXT( "" );

	// For externally launched minidump diagnostics.
	if( bUseCachedData )
	{
		TSet<FString> SymbolPaths;
		for( const auto& Filename : CrashInfo.PDBCacheEntry->Files )
		{
			const FString SymbolPath = FPaths::GetPath( Filename );
			if( SymbolPath.Len() > 0 )
			{
				SymbolPaths.Add( SymbolPath );
			}
		}

		for( const auto& SymbolPath : SymbolPaths )
		{
			CombinedPath += SymbolPath;
			CombinedPath += TEXT( ";" );
		}

		// Set the symbol path
		Symbol->SetImagePathWide( *CombinedPath );
		Symbol->SetSymbolPathWide( *CombinedPath );
	}
	// For locally launched minidump diagnostics.
	else
	{
		// We care only about the PDB and binary files.
		TArray<FString> FiletypesToInclude;
		FiletypesToInclude.Add( TEXT( ".pdb" ) );
		FiletypesToInclude.Add( TEXT( ".dll" ) );
		FiletypesToInclude.Add( TEXT( ".exe" ) );
		// These folders don't contain any binary files, so skip them to improve the finder performance.
		TArray<FString> DirectoriesToIgnore;
		DirectoriesToIgnore.Add( TEXT( "/Content/" ) );
		DirectoriesToIgnore.Add( TEXT( "/DerivedDataCache/" ) );
		DirectoriesToIgnore.Add( TEXT( "/Intermediate/" ) );
		DirectoriesToIgnore.Add( TEXT( "/Documentation/" ) );
		DirectoriesToIgnore.Add( TEXT( "/NoRedist/" ) );


		FSymbolAndExecutableFinder SymbolAndExecutableFinder( FPlatformFileManager::Get().GetPlatformFile(), FiletypesToInclude, DirectoriesToIgnore );
		SymbolAndExecutableFinder.Visit( *FPaths::RootDir(), true );


		TSet<FString> SymbolPaths;
		for( const auto& Filename : SymbolAndExecutableFinder.GetFilePaths() )
		{
			const FString Path = FPaths::GetPath( Filename );
			if( Path.Len() > 0 )
			{
				SymbolPaths.Add( Path );
			}
		}

		for( const auto& SymbolPath : SymbolPaths )
		{
			CombinedPath += SymbolPath;
			CombinedPath += TEXT( ";" );
		}

		// Set the symbol path
		Symbol->SetImagePathWide( *CombinedPath );
		Symbol->SetSymbolPathWide( *CombinedPath );
	}

	// Add in syncing of the Microsoft symbol servers if requested
	if( FParse::Param( FCommandLine::Get(), TEXT( "SyncMicrosoftSymbols" ) ) )
	{
		FString BinariesDir = FString(FPlatformProcess::BaseDir());
		if ( !FPaths::FileExists( FPaths::Combine(*BinariesDir, TEXT("symsrv.dll")) ) )
		{
			CrashInfo.Log( FString::Printf(TEXT("Error: symsrv.dll was not detected in '%s'. Microsoft symbols will not be downloaded!"), *BinariesDir) );
		}
		if ( !FPaths::FileExists( FPaths::Combine(*BinariesDir, TEXT("symsrv.yes")) ) )
		{
			CrashInfo.Log( FString::Printf(TEXT("symsrv.yes was not detected in '%s'. This will cause a popup to confirm the license."), *BinariesDir) );
		}
		if ( !FPaths::FileExists( FPaths::Combine(*BinariesDir, TEXT("dbghelp.dll")) ) )
		{
			CrashInfo.Log( FString::Printf(TEXT("Error: dbghelp.dll was not detected in '%s'. Microsoft symbols will not be downloaded!"), *BinariesDir) );
		}

		Symbol->AppendImagePathWide( TEXT( "SRV*..\\..\\Intermediate\\SymbolCache*http://msdl.microsoft.com/download/symbols" ) );
		Symbol->AppendSymbolPathWide( TEXT( "SRV*..\\..\\Intermediate\\SymbolCache*http://msdl.microsoft.com/download/symbols" ) );
	}

	TCHAR SymbolPath[1024] = { 0 };
	Symbol->GetSymbolPathWide( SymbolPath, 1024, NULL );
	CrashInfo.Log( FString::Printf( TEXT( " ... setting symbol path to: '%s'" ), SymbolPath ) );

	Symbol->GetImagePathWide( SymbolPath, 1024, NULL );
	CrashInfo.Log( FString::Printf( TEXT( " ... setting image path to: '%s'" ), SymbolPath ) );
}

class FSortModulesByName
{
public:
	bool operator()( const FCrashModuleInfo& A, const FCrashModuleInfo& B ) const
	{
		if( A.Extension == TEXT( ".exe" ) )
		{
			return true;
		}

		if( A.Extension != B.Extension )
		{
			// Sort any exe modules to the top
			return ( A.Extension > B.Extension );
		}

		// alphabetise all the dlls
		return ( A.Name < B.Name );
	}
};

/** 
 * Get detailed info about each module
 */
void FWindowsPlatformStackWalkExt::GetModuleInfoDetailed()
{
	// The the number of loaded modules
	ULONG LoadedModuleCount = 0;
	ULONG UnloadedModuleCount = 0;
	Symbol->GetNumberModules( &LoadedModuleCount, &UnloadedModuleCount );

	CrashInfo.Modules.Empty( LoadedModuleCount );
	for( uint32 ModuleIndex = 0; ModuleIndex < LoadedModuleCount; ModuleIndex++ )
	{
		FCrashModuleInfo* CrashModule = new ( CrashInfo.Modules ) FCrashModuleInfo();

		ULONG64 ModuleBase = 0;
		Symbol->GetModuleByIndex( ModuleIndex, &ModuleBase );

		// Get the full path of the module name
		TCHAR ModuleName[MAX_PATH] = { 0 };
		Symbol->GetModuleNameStringWide( DEBUG_MODNAME_IMAGE, ModuleIndex, ModuleBase, ModuleName, MAX_PATH, NULL );

		FString RelativeModuleName = ExtractRelativePath( TEXT( "binaries" ), ModuleName );
		if( RelativeModuleName.Len() > 0 )
		{
			CrashModule->Name = RelativeModuleName;
		}
		else
		{
			CrashModule->Name = ModuleName;
		}
		CrashModule->Extension = CrashModule->Name.Right( 4 ).ToLower();
		CrashModule->BaseOfImage = ModuleBase;

		DEBUG_MODULE_PARAMETERS ModuleParameters = { 0 };
		Symbol->GetModuleParameters( 1, NULL, ModuleIndex, &ModuleParameters ); 
		CrashModule->SizeOfImage = ModuleParameters.Size;

		VS_FIXEDFILEINFO VersionInfo = { 0 };
		Symbol->GetModuleVersionInformationWide( ModuleIndex, ModuleBase, TEXT( "\\" ), &VersionInfo, sizeof( VS_FIXEDFILEINFO ), NULL );

		CrashModule->Major = HIWORD( VersionInfo.dwProductVersionMS );
		CrashModule->Minor = LOWORD( VersionInfo.dwProductVersionMS );
		CrashModule->Patch = HIWORD( VersionInfo.dwProductVersionLS );
		CrashModule->Revision = LOWORD( VersionInfo.dwProductVersionLS );

		// Ensure all the images are synced - need the full path here
		Symbol->ReloadWide( *CrashModule->Name );
	}

	CrashInfo.Modules.Sort( FSortModulesByName() );
}

/**
 * Check to see if the stack address resides within one of the loaded modules i.e. is it valid?
 */
bool FWindowsPlatformStackWalkExt::IsOffsetWithinModules( uint64 Offset )
{
	for( int ModuleIndex = 0; ModuleIndex < CrashInfo.Modules.Num(); ModuleIndex++ )
	{
		FCrashModuleInfo& CrashModule = CrashInfo.Modules[ModuleIndex];
		if( Offset >= CrashModule.BaseOfImage && Offset < CrashModule.BaseOfImage + CrashModule.SizeOfImage )
		{
			return true;
		}
	}

	return false;
}

/** 
 * Extract the system info of the crash from the minidump
 */
void FWindowsPlatformStackWalkExt::GetSystemInfo()
{
	FCrashSystemInfo& SystemInfo = CrashInfo.SystemInfo;

	ULONG PlatformId = 0;
	ULONG Major = 0;
	ULONG Minor = 0;
	ULONG Build = 0;
	ULONG Revision = 0;
	Control->GetSystemVersionValues( &PlatformId, &Major, &Minor, &Build, &Revision );

	SystemInfo.OSMajor = ( uint16 )Major;
	SystemInfo.OSMinor = ( uint16 )Minor;
	SystemInfo.OSBuild = ( uint16 )Build;
	SystemInfo.OSRevision = ( uint16 )Revision;

	ULONG ProcessorType = 0;
	Control->GetActualProcessorType( &ProcessorType );

	switch( ProcessorType )
	{
	case IMAGE_FILE_MACHINE_I386:
		// x86
		CrashInfo.SystemInfo.ProcessorArchitecture = PA_X86;
		break;

	case IMAGE_FILE_MACHINE_ARM:
		// ARM
		CrashInfo.SystemInfo.ProcessorArchitecture = PA_ARM;
		break;

	case IMAGE_FILE_MACHINE_AMD64:
		// x64
		CrashInfo.SystemInfo.ProcessorArchitecture = PA_X64;
		break;

	default: 
		break;
	};

	ULONG ProcessorCount = 0;
	Control->GetNumberProcessors( &ProcessorCount );
	SystemInfo.ProcessorCount = ProcessorCount;
}

/** 
 * Extract the thread info from the minidump
 */
void FWindowsPlatformStackWalkExt::GetThreadInfo()
{
}

/** 
 * Extract info about the exception that caused the crash
 */
void FWindowsPlatformStackWalkExt::GetExceptionInfo()
{
	FCrashExceptionInfo& Exception = CrashInfo.Exception;

	ULONG ExceptionType = 0;
	ULONG ProcessID = 0;
	ULONG ThreadId = 0;
	TCHAR Description[MAX_PATH] = { 0 };
	Control->GetLastEventInformationWide( &ExceptionType, &ProcessID, &ThreadId, NULL, 0, NULL, Description, sizeof( Description ), NULL );

	Exception.Code = ExceptionType;
	Exception.ProcessId = ProcessID;
	Exception.ThreadId = ThreadId;
	Exception.ExceptionString = Description;
}

/** 
 * Get the callstack of the crash. Returns true if at least one function name was found in the debugging symbols
 */
bool FWindowsPlatformStackWalkExt::GetCallstacks()
{
	bool bAtLeastOneFunctionNameFound = false;

	FCrashExceptionInfo& Exception = CrashInfo.Exception;

	byte Context[4096] = { 0 };
	ULONG DebugEvent = 0;
	ULONG ProcessID = 0;
	ULONG ThreadID = 0;
	ULONG ContextSize = 0;

	// Get the context of the crashed thread
	if( Control->GetStoredEventInformation( &DebugEvent, &ProcessID, &ThreadID, Context, sizeof( Context ), &ContextSize, NULL, 0, 0) )
	{
		return bAtLeastOneFunctionNameFound;
	}

	// Some magic number checks
	if( ContextSize == 716 )
	{
		CrashInfo.Log( FString::Printf( TEXT( " ... context size matches x86 sizeof( CONTEXT )" ) ) );
	}
	else if( ContextSize == 1232 )
	{
		CrashInfo.Log( FString::Printf( TEXT( " ... context size matches x64 sizeof( CONTEXT )" ) ) );
	}

	// Get the entire stack trace
	const uint32 MaxFrames = 128;
	const uint32 MaxFramesSize = MaxFrames * ContextSize;

	DEBUG_STACK_FRAME StackFrames[MaxFrames] = { 0 };
	ULONG Count = 0;
	bool bFoundSourceFile = false;
	void* ContextData = FMemory::Malloc( MaxFramesSize );
	FMemory::Memzero( ContextData, MaxFramesSize );
	HRESULT HR = Control->GetContextStackTrace( Context, ContextSize, StackFrames, MaxFrames, ContextData, MaxFramesSize, ContextSize, &Count );

	for( uint32 StackIndex = 0; StackIndex < Count; StackIndex++ )
	{
		FString Line;
		uint64 Offset = StackFrames[StackIndex].InstructionOffset;

		if( IsOffsetWithinModules( Offset ) )
		{
			// Get the module, function, and offset
			uint64 Displacement = 0;
			TCHAR FunctionName[MAX_PATH] = { 0 };

			Symbol->GetNameByOffsetWide( Offset, FunctionName, ARRAYSIZE( FunctionName ) - 1, NULL, &Displacement );

			// Look for source file name and line number
			TCHAR SourceName[MAX_PATH] = { 0 };
			ULONG LineNumber = 0;

			Symbol->GetLineByOffsetWide( Offset, &LineNumber, SourceName, ARRAYSIZE( SourceName ) - 1, NULL, NULL );

			Line = FunctionName;

			// Remember the top of the stack to locate in the source file
			if( !bFoundSourceFile && FString( SourceName ).Len() > 0 && LineNumber > 0 )
			{
				CrashInfo.SourceFile = ExtractRelativePath( TEXT( "source" ), SourceName );
				CrashInfo.SourceLineNumber = LineNumber;
				bFoundSourceFile = true;
			}

			// If we find an assert, the actual source file we're interested in is the next one up, so reset the source file found flag
			if( Line.Contains( TEXT( "FDebug::AssertFailed" ), ESearchCase::CaseSensitive)
				|| Line.Contains( TEXT("FDebug::EnsureNotFalse"), ESearchCase::CaseSensitive)
				|| Line.Contains( TEXT( "FDebug::EnsureFailed" ), ESearchCase::CaseSensitive) )
			{
				bFoundSourceFile = false;
			}

			// According to MSDN, the symbol name will include an ! if the function name could be discovered, delimiting it from the module name
			if( Line.Contains(TEXT( "!" )) )
			{
				Line += TEXT( "()" );
				bAtLeastOneFunctionNameFound = true;
			}

			Line += FString::Printf( TEXT( " + %d bytes" ), Displacement );
			
			if( LineNumber > 0 )
			{
				Line += FString::Printf( TEXT( " [%s:%d]" ), SourceName, LineNumber );
			}

			Exception.CallStackString.Add( Line );
		}
		else
		{
			break;
		}

		// Don't care about any more entries higher than this
		if( Line.Contains(TEXT( "tmainCRTStartup" )) )
		{
			break;
		}
	}

	FMemory::Free( ContextData );

	CrashInfo.Log( FString::Printf( TEXT( " ... callstack generated!" ) ) );

	return bAtLeastOneFunctionNameFound;
}

/** 
 * Open a minidump as a new session
 */
bool FWindowsPlatformStackWalkExt::OpenDumpFile( const FString& InCrashDumpFilename )
{
	const bool bFound = IFileManager::Get().FileSize( *InCrashDumpFilename ) != INDEX_NONE;
	if( !bFound )
	{
		CrashInfo.Log( FString::Printf( TEXT( " ... failed to find minidump file: %s" ), *InCrashDumpFilename ) );
		return false;
	}

	if( Client->OpenDumpFileWide( *InCrashDumpFilename, NULL ) )
	{
		CrashInfo.Log( FString::Printf( TEXT( " ... failed to open minidump: %s" ), *InCrashDumpFilename ) );
		return false;
	}

	if( Control->WaitForEvent( 0, INFINITE ) != S_OK )
	{
		CrashInfo.Log( FString::Printf( TEXT( " ... failed while waiting for minidump to load: %s" ), *InCrashDumpFilename ) );
		return false;
	}

	CrashInfo.Log( FString::Printf( TEXT( "Successfully opened minidump: %s" ), *InCrashDumpFilename ) );
	return true;
}
