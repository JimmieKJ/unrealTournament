// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidPlatformStackWalk.cpp: Android implementations of stack walk functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include <unwind.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <stdio.h>

bool FAndroidPlatformStackWalk::ProgramCounterToHumanReadableString(int32 CurrentCallDepth, uint64 ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, FGenericCrashContext* Context)
{
	Dl_info DylibInfo;
	int32 Result = dladdr((const void*)ProgramCounter, &DylibInfo);
	if (Result == 0)
	{
		return false;
	}

	FProgramCounterSymbolInfo SymbolInfo;
	ProgramCounterToSymbolInfo(ProgramCounter, SymbolInfo);

	// Write out function name.
	FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, SymbolInfo.FunctionName);

	// Get filename.
	{
		ANSICHAR FileNameLine[MAX_SPRINTF];
		if (SymbolInfo.LineNumber == 0)
		{
			// No line number. Print out the module offset address instead.
			FCStringAnsi::Sprintf(FileNameLine, "Address = 0x%-8x ", SymbolInfo.OffsetInModule);
		}
		else
		{
			// try to add source file and line number, too
			FCStringAnsi::Sprintf(FileNameLine, "[%s, line %d] ", SymbolInfo.Filename, SymbolInfo.LineNumber);
		}

		FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, FileNameLine);
	}

	// Get module name.
	{
		ANSICHAR ModuleName[MAX_SPRINTF];
		// Write out Module information if there is sufficient space.
		FCStringAnsi::Sprintf(ModuleName, "[in %s]", SymbolInfo.ModuleName);
		FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, ModuleName);
	}

	// For the crash reporting code this needs a Windows line ending, the caller is responsible for the '\n'
	FCStringAnsi::Strcat(HumanReadableString, HumanReadableStringSize, "\r");

	return true;
}

void FAndroidPlatformStackWalk::ProgramCounterToSymbolInfo(uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo)
{
	Dl_info DylibInfo;
	int32 Result = dladdr((const void*)ProgramCounter, &DylibInfo);
	if (Result == 0)
	{
		return;
	}

	int32 Status = 0;
	ANSICHAR* DemangledName = NULL;

	// Increased the size of the demangle destination to reduce the chances that abi::__cxa_demangle will allocate
	// this causes the app to hang as malloc isn't signal handler safe. Ideally we wouldn't call this function in a handler.
	size_t DemangledNameLen = 65536;
	ANSICHAR DemangledNameBuffer[65536] = { 0 };
	DemangledName = abi::__cxa_demangle(DylibInfo.dli_sname, DemangledNameBuffer, &DemangledNameLen, &Status);

	if (DemangledName)
	{
		// C++ function
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "%s ", DemangledName);
	}
	else if (DylibInfo.dli_sname)
	{
		// C function
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "%s() ", DylibInfo.dli_sname);
	}
	else
	{
		// Unknown!
		FCStringAnsi::Sprintf(out_SymbolInfo.FunctionName, "[Unknown]() ");
	}

	// No line number available.
	// TODO open libUE4.so from the apk and get the DWARF-2 data.
	FCStringAnsi::Strcat(out_SymbolInfo.Filename, "Unknown");
	out_SymbolInfo.LineNumber = 0;
	
	// Offset of the symbol in the module, eg offset into libUE4.so needed for offline addr2line use.
	out_SymbolInfo.OffsetInModule = ProgramCounter - (uint64)DylibInfo.dli_fbase;

	// Write out Module information.
	ANSICHAR* DylibPath = (ANSICHAR*)DylibInfo.dli_fname;
	ANSICHAR* DylibName = FCStringAnsi::Strrchr(DylibPath, '/');
	if (DylibName)
	{
		DylibName += 1;
	}
	else
	{
		DylibName = DylibPath;
	}
	FCStringAnsi::Strcpy(out_SymbolInfo.ModuleName, DylibName);
}

namespace AndroidStackWalkHelpers
{
	uint64* BackTrace;
	uint32 MaxDepth;

	static _Unwind_Reason_Code BacktraceCallback(struct _Unwind_Context* Context, void* InDepthPtr)
	{
		uint32* DepthPtr = (uint32*)InDepthPtr;

		if (*DepthPtr < MaxDepth)
		{
			BackTrace[*DepthPtr] = (uint64)_Unwind_GetIP(Context);
		}

		(*DepthPtr)++;
		return (_Unwind_Reason_Code)0;
	}
}

void FAndroidPlatformStackWalk::CaptureStackBackTrace(uint64* BackTrace, uint32 MaxDepth, void* Context)
{
	// Make sure we have place to store the information
	if (BackTrace == NULL || MaxDepth == 0)
	{
		return;
	}

	// zero results
	FPlatformMemory::Memzero(BackTrace, MaxDepth*sizeof(uint64));

	// backtrace
	AndroidStackWalkHelpers::BackTrace = BackTrace;
	AndroidStackWalkHelpers::MaxDepth = MaxDepth;
	uint32 Depth = 0;
	_Unwind_Backtrace(AndroidStackWalkHelpers::BacktraceCallback, &Depth);
}
