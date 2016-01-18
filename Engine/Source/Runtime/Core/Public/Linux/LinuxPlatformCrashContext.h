// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformCrashContext.h"

// commandline parameter to suppress DWARF parsing (greatly speeds up callstack generation)
#define CMDARG_SUPPRESS_DWARF_PARSING			"nodwarf"

struct CORE_API FLinuxCrashContext : public FGenericCrashContext
{
	/** Signal number */
	int32 Signal;
	
	/** Additional signal info */
	siginfo_t* Info;
	
	/** Thread context */
	ucontext_t*	Context;

	/** Symbols received via backtrace_symbols(), if any (note that we will need to clean it up) */
	char ** BacktraceSymbols;

	/** Memory reserved for "exception" (signal) info */
	TCHAR SignalDescription[128];

	/** Memory reserved for minidump-style callstack info */
	char MinidumpCallstackInfo[16384];

	FLinuxCrashContext()
		:	Signal(0)
		,	Info(NULL)
		,	Context(NULL)
		,	BacktraceSymbols(NULL)
	{
		SignalDescription[ 0 ] = 0;
		MinidumpCallstackInfo[ 0 ] = 0;
	}

	~FLinuxCrashContext();

	/**
	 * Inits the crash context from data provided by a signal handler.
	 *
	 * @param InSignal number (SIGSEGV, etc)
	 * @param InInfo additional info (e.g. address we tried to read, etc)
	 * @param InContext thread context
	 */
	void InitFromSignal(int32 InSignal, siginfo_t* InInfo, void* InContext);

	/**
	 * Dumps all the data from crash context to the "minidump" report.
	 *
	 * @param DiagnosticsPath Path to put the file to
	 */
	void GenerateReport(const FString & DiagnosticsPath) const;
};

typedef FLinuxCrashContext FPlatformCrashContext;
