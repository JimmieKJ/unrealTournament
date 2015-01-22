// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

static const ANSICHAR* GUnknownFunction = "UnknownFunction";

FProgramCounterSymbolInfo::FProgramCounterSymbolInfo() :
	LineNumber( 0 ),
	SymbolDisplacement( 0 ),
	OffsetInModule( 0 ),
	ProgramCounter( 0 )
{
	FCStringAnsi::Strncpy( ModuleName, "UnknownModule", MAX_NAME_LENGHT );
	FCStringAnsi::Strncpy( FunctionName, GUnknownFunction, MAX_NAME_LENGHT );
	FCStringAnsi::Strncpy( Filename, "UnknownFile", MAX_NAME_LENGHT );
}

bool FGenericPlatformStackWalk::ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context )
{
	if (HumanReadableString && HumanReadableStringSize > 0)
	{
		FProgramCounterSymbolInfo SymbolInfo;
		FPlatformStackWalk::ProgramCounterToSymbolInfo( ProgramCounter, SymbolInfo );

		// ModuleName!FunctionName (ProgramCounter) + offset bytes [Filename:LineNumber]
		ANSICHAR StackLine[MAX_SPRINTF];

		// Strip module path.
		const ANSICHAR* Pos0 = FCStringAnsi::Strrchr( SymbolInfo.ModuleName, '\\' );
		const ANSICHAR* Pos1 = FCStringAnsi::Strrchr( SymbolInfo.ModuleName, '/' );
		const UPTRINT RealPos = FMath::Max( (UPTRINT)Pos0, (UPTRINT)Pos1 );
		const ANSICHAR* StrippedModuleName = RealPos > 0 ? (const ANSICHAR*)(RealPos+1) : SymbolInfo.ModuleName;
	
		FCStringAnsi::Sprintf( StackLine, "%s!%s (0x%016llx) + %i bytes [%s:%i]", StrippedModuleName, (const ANSICHAR*)SymbolInfo.FunctionName, SymbolInfo.ProgramCounter, SymbolInfo.SymbolDisplacement, (const ANSICHAR*)SymbolInfo.Filename, SymbolInfo.LineNumber );

		// Append the stack line.
		FCStringAnsi::Strcat( HumanReadableString, HumanReadableStringSize, StackLine );

		// Return true, if we found a valid function name.
		return FCStringAnsi::Strncmp( SymbolInfo.FunctionName, GUnknownFunction, FProgramCounterSymbolInfo::MAX_NAME_LENGHT ) != 0;
	}
	return false;
}

void FGenericPlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
{

}

void FGenericPlatformStackWalk::StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context )
{
	// Temporary memory holding the stack trace.
	static const int MAX_DEPTH = 100;
	uint64 StackTrace[MAX_DEPTH];
	FMemory::MemZero( StackTrace );

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

