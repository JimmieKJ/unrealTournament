// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "GenericPlatformContext.h"

FProgramCounterSymbolInfo::FProgramCounterSymbolInfo() :
	LineNumber( 0 ),
	SymbolDisplacement( 0 ),
	OffsetInModule( 0 ),
	ProgramCounter( 0 )
{
	FCStringAnsi::Strncpy( ModuleName, "", MAX_NAME_LENGHT );
	FCStringAnsi::Strncpy( FunctionName, "", MAX_NAME_LENGHT );
	FCStringAnsi::Strncpy( Filename, "", MAX_NAME_LENGHT );
}

bool FGenericPlatformStackWalk::ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context )
{
	if (HumanReadableString && HumanReadableStringSize > 0)
	{
		FProgramCounterSymbolInfo SymbolInfo;
		FPlatformStackWalk::ProgramCounterToSymbolInfo( ProgramCounter, SymbolInfo );

		return SymbolInfoToHumanReadableString( SymbolInfo, HumanReadableString, HumanReadableStringSize );
	}
	return false;
}

bool FGenericPlatformStackWalk::SymbolInfoToHumanReadableString( const FProgramCounterSymbolInfo& SymbolInfo, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize )
{
	const int32 MAX_TEMP_SPRINTF = 64;
	// Valid callstack line 
	// ModuleName!FunctionName {ProgramCounter} + offset bytes [Filename:LineNumber]
	// 
	// Invalid callstack line
	// ModuleName! {ProgramCounter} + offset bytes
	// 
	if( HumanReadableString && HumanReadableStringSize > 0 )
	{
		ANSICHAR StackLine[MAX_SPRINTF] = {0};

		// Strip module path.
		const ANSICHAR* Pos0 = FCStringAnsi::Strrchr( SymbolInfo.ModuleName, '\\' );
		const ANSICHAR* Pos1 = FCStringAnsi::Strrchr( SymbolInfo.ModuleName, '/' );
		const UPTRINT RealPos = FMath::Max( (UPTRINT)Pos0, (UPTRINT)Pos1 );
		const ANSICHAR* StrippedModuleName = RealPos > 0 ? (const ANSICHAR*)(RealPos + 1) : SymbolInfo.ModuleName;

		//FCStringAnsi::Sprintf( StackLine, "%s!%s {0x%016llx} + %i bytes [%s:%i]", StrippedModuleName, (const ANSICHAR*)SymbolInfo.FunctionName, SymbolInfo.ProgramCounter, SymbolInfo.SymbolDisplacement, (const ANSICHAR*)SymbolInfo.Filename, SymbolInfo.LineNumber );
		FCStringAnsi::Strcat( StackLine, MAX_SPRINTF, StrippedModuleName );
		
		const bool bHasValidFunctionName = FCStringAnsi::Strlen( SymbolInfo.FunctionName ) > 0;
		if( bHasValidFunctionName )
		{
			FCStringAnsi::Strcat( StackLine, MAX_SPRINTF, "!" );
			FCStringAnsi::Strcat( StackLine, MAX_SPRINTF, SymbolInfo.FunctionName );
		}

		{
			ANSICHAR ProgramCounterAndSymbolDisplacement[MAX_TEMP_SPRINTF] = {0};
			FCStringAnsi::Snprintf( ProgramCounterAndSymbolDisplacement, MAX_TEMP_SPRINTF, " {0x%016llx} + %i bytes", SymbolInfo.ProgramCounter, SymbolInfo.SymbolDisplacement );
			FCStringAnsi::Strcat( StackLine, MAX_SPRINTF, ProgramCounterAndSymbolDisplacement );
		}

		const bool bHasValidFilename = FCStringAnsi::Strlen( SymbolInfo.Filename ) > 0 && SymbolInfo.LineNumber > 0;
		if( bHasValidFilename )
		{
			ANSICHAR FilenameAndLineNumber[MAX_TEMP_SPRINTF] = {0};
			FCStringAnsi::Snprintf( FilenameAndLineNumber, MAX_TEMP_SPRINTF, " [%s:%i]", SymbolInfo.Filename, SymbolInfo.LineNumber );
			FCStringAnsi::Strcat( StackLine, MAX_SPRINTF, FilenameAndLineNumber );
		}


		// Append the stack line.
		FCStringAnsi::Strcat( HumanReadableString, HumanReadableStringSize, StackLine );

		// Return true, if we have a valid function name.
		return bHasValidFunctionName;
	}
	return false;
}


bool FGenericPlatformStackWalk::SymbolInfoToHumanReadableStringEx( const FProgramCounterSymbolInfoEx& SymbolInfo, FString& out_HumanReadableString )
{
	// Valid callstack line 
	// ModuleName!FunctionName {ProgramCounter} + offset bytes [Filename:LineNumber]
	// 
	// Invalid callstack line
	// ModuleName! {ProgramCounter} + offset bytes
	
	// Strip module path.
	const TCHAR* Pos0 = FCString::Strrchr( *SymbolInfo.ModuleName, '\\' );
	const TCHAR* Pos1 = FCString::Strrchr( *SymbolInfo.ModuleName, '/' );
	const UPTRINT RealPos = FMath::Max( (UPTRINT)Pos0, (UPTRINT)Pos1 );
	const FString StrippedModuleName = RealPos > 0 ? (const ANSICHAR*)(RealPos + 1) : SymbolInfo.ModuleName;

	out_HumanReadableString = StrippedModuleName;
	
	const bool bHasValidFunctionName = SymbolInfo.FunctionName.Len() > 0;
	if( bHasValidFunctionName )
	{	
		out_HumanReadableString += TEXT( "!" );
		out_HumanReadableString += SymbolInfo.FunctionName;
	}

	//out_HumanReadableString += FString::Printf( TEXT( " {0x%016llx} + %i bytes" ), SymbolInfo.ProgramCounter, SymbolInfo.SymbolDisplacement );

	const bool bHasValidFilename = SymbolInfo.Filename.Len() > 0 && SymbolInfo.LineNumber > 0;
	if( bHasValidFilename )
	{
		out_HumanReadableString += FString::Printf( TEXT( " [%s:%i]" ), *SymbolInfo.Filename, SymbolInfo.LineNumber );
	}

	// Return true, if we have a valid function name.
	return bHasValidFunctionName;
}


void FGenericPlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
{

}

void FGenericPlatformStackWalk::StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context )
{
	// Temporary memory holding the stack trace.
	static const int MAX_DEPTH = 100;
	uint64 StackTrace[MAX_DEPTH];
	FMemory::Memzero( StackTrace );

	// Capture stack backtrace.
	FPlatformStackWalk::CaptureStackBackTrace( StackTrace, MAX_DEPTH, Context );

	// Skip the first two entries as they are inside the stack walking code.
	int32 CurrentDepth = IgnoreCount;
	// Allow the first entry to be NULL as the crash could have been caused by a call to a NULL function pointer,
	// which would mean the top of the callstack is NULL.
	while( StackTrace[CurrentDepth] || ( CurrentDepth == IgnoreCount ) )
	{
		FPlatformStackWalk::ProgramCounterToHumanReadableString( CurrentDepth, StackTrace[CurrentDepth], HumanReadableString, HumanReadableStringSize, reinterpret_cast< FGenericCrashContext* >( Context ) );
		FCStringAnsi::Strcat( HumanReadableString, HumanReadableStringSize, LINE_TERMINATOR_ANSI );
		CurrentDepth++;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("CrashForUAT")) && FParse::Param(FCommandLine::Get(), TEXT("stdout")))
	{
		FPlatformMisc::LowLevelOutputDebugString(ANSI_TO_TCHAR(HumanReadableString));
		wprintf(TEXT("\nbegin: stack for UAT"));
		wprintf(TEXT("\n%s"), ANSI_TO_TCHAR(HumanReadableString));
		wprintf(TEXT("\nend: stack for UAT"));
		fflush(stdout);
	}
}

