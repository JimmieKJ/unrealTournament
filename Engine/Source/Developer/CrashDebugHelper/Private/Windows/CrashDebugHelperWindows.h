// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrashDebugHelper.h"

class FCrashDebugHelperWindows : public ICrashDebugHelper
{
public:
	FCrashDebugHelperWindows();
	virtual ~FCrashDebugHelperWindows();

	/**
	 *	Parse the given crash dump, determining EngineVersion of the build that produced it - if possible. 
	 *
	 *	@param	InCrashDumpName		The crash dump file to process
	 *	@param	OutCrashDebugInfo	The crash dump info extracted from the file
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool ParseCrashDump(const FString& InCrashDumpName, FCrashDebugInfo& OutCrashDebugInfo) override;

	/**
	 *	Parse the given crash dump, and generate a report. 
	 *
	 *	@param	InCrashDumpName		The crash dump file to process
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool CreateMinidumpDiagnosticReport( const FString& InCrashDumpName ) override;
};

typedef FCrashDebugHelperWindows FCrashDebugHelper;
