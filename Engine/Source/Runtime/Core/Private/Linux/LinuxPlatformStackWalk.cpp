// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxPlatformStackWalk.cpp: Linux implementations of stack walk functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "Misc/App.h"
#include "LinuxPlatformCrashContext.h"
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <stdio.h>

namespace LinuxStackWalkHelpers
{
	enum
	{
		MaxMangledNameLength = 1024,
		MaxDemangledNameLength = 1024
	};

	/**
	 * Finds mangled name and returns it in internal buffer.
	 * Caller doesn't have to deallocate that.
	 */
	const char * GetMangledName(const char * SourceInfo)
	{
		static char MangledName[MaxMangledNameLength + 1];
		const char * Current = SourceInfo;

		MangledName[0] = 0;
		if (Current == NULL)
		{
			return MangledName;
		}

		// find '('
		for (; *Current != 0 && *Current != '('; ++Current);

		// if unable to find, return original
		if (*Current == 0)
		{
			return SourceInfo;
		}

		// copy everything until '+'
		++Current;
		size_t BufferIdx = 0;
		for (; *Current != 0 && *Current != '+' && BufferIdx < MaxMangledNameLength; ++Current, ++BufferIdx)
		{
			MangledName[BufferIdx] = *Current;
		}

		// if unable to find, return original
		if (*Current == 0)
		{
			return SourceInfo;
		}

		MangledName[BufferIdx] = 0;
		return MangledName;
	}

	/**
	 * Returns source filename for particular callstack depth (or NULL).
	 * Caller doesn't have to deallocate that.
	 */
	const char * GetFunctionName(FGenericCrashContext* Context, int32 CurrentCallDepth)
	{
		static char DemangledName[MaxDemangledNameLength + 1];
		if (Context == NULL || CurrentCallDepth < 0)
		{
			return NULL;
		}

		FLinuxCrashContext* LinuxContext = static_cast< FLinuxCrashContext* >( Context );

		if (LinuxContext->BacktraceSymbols == NULL)
		{
			return NULL;
		}

		const char * SourceInfo = LinuxContext->BacktraceSymbols[CurrentCallDepth];
		if (SourceInfo == NULL)
		{
			return NULL;
		}

		// see http://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html for C++ demangling
		int DemangleStatus = 0xBAD;
		char * Demangled = abi::__cxa_demangle(GetMangledName( SourceInfo ), NULL, NULL, &DemangleStatus);
		if (Demangled != NULL && DemangleStatus == 0)
		{
			FCStringAnsi::Strncpy(DemangledName, Demangled, ARRAY_COUNT(DemangledName) - 1);
		}
		else
		{
			FCStringAnsi::Strncpy(DemangledName, SourceInfo, ARRAY_COUNT(DemangledName) - 1);
		}

		if (Demangled)
		{
			free(Demangled);
		}
		return DemangledName;
	}

	void AppendToString(ANSICHAR * HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext * Context, const ANSICHAR * Text)
	{
		FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, Text);
	}

	void AppendFunctionNameIfAny(FLinuxCrashContext & LinuxContext, const char * FunctionName, uint64 ProgramCounter)
	{
		if (FunctionName)
		{
			FCStringAnsi::Strcat(LinuxContext.MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext.MinidumpCallstackInfo ) - 1, FunctionName);
			FCStringAnsi::Strcat(LinuxContext.MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext.MinidumpCallstackInfo ) - 1, " + some bytes");	// this is just to conform to crashreporterue4 standard
		}
		else
		{
			ANSICHAR TempArray[MAX_SPRINTF];

			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "0x%016llx", ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "0x%08x", (uint32)ProgramCounter);
			}
			FCStringAnsi::Strcat(LinuxContext.MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext.MinidumpCallstackInfo ) - 1, TempArray);
		}
	}
}

void FLinuxPlatformStackWalk::ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo )
{
	// Set the program counter.
	out_SymbolInfo.ProgramCounter = ProgramCounter;

	// Get function, filename and line number.
	// @TODO
}

bool FLinuxPlatformStackWalk::ProgramCounterToHumanReadableString( int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context )
{
	if (HumanReadableString && HumanReadableStringSize > 0)
	{
		ANSICHAR TempArray[MAX_SPRINTF];
		if (CurrentCallDepth < 0)
		{
			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "[Callstack] 0x%016llx ", ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "[Callstack] 0x%08x ", (uint32) ProgramCounter);
			}
			LinuxStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);

			// won't be able to display names here
		}
		else
		{
			if (PLATFORM_64BITS)
			{
				FCStringAnsi::Sprintf(TempArray, "[Callstack]  %02d  0x%016llx  ", CurrentCallDepth, ProgramCounter);
			}
			else
			{
				FCStringAnsi::Sprintf(TempArray, "[Callstack]  %02d  0x%08x  ", CurrentCallDepth, (uint32) ProgramCounter);
			}
			LinuxStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);

			// Get filename.
			{
				const char * FunctionName = LinuxStackWalkHelpers::GetFunctionName(Context, CurrentCallDepth);
				if (FunctionName)
				{
					LinuxStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, FunctionName);
				}

				// try to add source file and line number, too
				FLinuxCrashContext* LinuxContext = static_cast< FLinuxCrashContext* >( Context );
				if (LinuxContext)
				{
					const char * SourceFilename = NULL;
					int LineNumber;
					if (LinuxContext->GetInfoForAddress(reinterpret_cast< void* >( ProgramCounter ), NULL, &SourceFilename, &LineNumber) && FunctionName != NULL)
					{
						FCStringAnsi::Sprintf(TempArray, " [%s, line %d]", SourceFilename, LineNumber);
						LinuxStackWalkHelpers::AppendToString(HumanReadableString, HumanReadableStringSize, Context, TempArray);
						FCStringAnsi::Sprintf(TempArray, " [%s:%d]", SourceFilename, LineNumber);

						// if we were able to find info for this line, it means it's in our code
						// append program name (strong assumption of monolithic build here!)						
						FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, TCHAR_TO_ANSI(*FApp::GetName()));
						FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, "!");
						LinuxStackWalkHelpers::AppendFunctionNameIfAny(*LinuxContext, FunctionName, ProgramCounter);
						FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, TempArray);
					}
					else
					{
						// if we were NOT able to find info for this line, it means it's something else
						FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, "Unknown!");
						LinuxStackWalkHelpers::AppendFunctionNameIfAny(*LinuxContext, FunctionName, ProgramCounter);
					}
					FCStringAnsi::Strcat(LinuxContext->MinidumpCallstackInfo, ARRAY_COUNT( LinuxContext->MinidumpCallstackInfo ) - 1, "\r\n");	// this one always uses Windows line terminators
				}
			}
		}
		return true;
	}
	return true;
}

void FLinuxPlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
{
	// Make sure we have place to store the information before we go through the process of raising
	// an exception and handling it.
	if( BackTrace == NULL || MaxDepth == 0 )
	{
		return;
	}

	size_t size = backtrace(reinterpret_cast< void** >( BackTrace ), MaxDepth);

	if (Context)
	{
		FLinuxCrashContext* LinuxContext = reinterpret_cast< FLinuxCrashContext* >( Context );

		if (LinuxContext->BacktraceSymbols == NULL)
		{
			// @TODO yrx 2014-09-29 Replace with backtrace_symbols_fd due to malloc()
			LinuxContext->BacktraceSymbols = backtrace_symbols(reinterpret_cast< void** >( BackTrace ), MaxDepth);
		}
	}
}

void NewReportEnsure( const TCHAR* ErrorMessage )
{
}
