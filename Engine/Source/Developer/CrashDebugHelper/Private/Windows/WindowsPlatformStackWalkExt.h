// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#undef PLATFORM_SUPPORTS_STACK_SYMBOLS
#define PLATFORM_SUPPORTS_STACK_SYMBOLS 1

class FCrashInfo;
class FCrashModuleInfo;

/**
 *	Windows implementation of stack walking using the COM interface IDebugClient5. 
 */
struct FWindowsPlatformStackWalkExt
{
	/** Default constructor. */
	FWindowsPlatformStackWalkExt( FCrashInfo& InCrashInfo );

	/** Destructor. */
	~FWindowsPlatformStackWalkExt();

	bool InitStackWalking();
	void ShutdownStackWalking();

	void GetExeFileVersionAndModuleList( FCrashModuleInfo& out_ExeFileVersion );
	void InitSymbols();
	bool OpenDumpFile( const FString& DumpFileName );
	void SetSymbolPathsFromModules();
	void GetModuleInfoDetailed();
	void GetSystemInfo();
	void GetThreadInfo();
	void GetExceptionInfo();
	bool GetCallstacks();

	bool IsOffsetWithinModules( uint64 Offset );

	static FString ExtractRelativePath( const TCHAR* BaseName, TCHAR* FullName );

protected:
	/** Reference to the crash info. */
	FCrashInfo& CrashInfo;
};
