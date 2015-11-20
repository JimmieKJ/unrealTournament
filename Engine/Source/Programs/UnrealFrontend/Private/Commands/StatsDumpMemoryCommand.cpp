// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealFrontendPrivatePCH.h"
#include "StatsDumpMemoryCommand.h"
#include "Profiler.h"

/*-----------------------------------------------------------------------------
	FStatsMemoryDumpCommand
-----------------------------------------------------------------------------*/

void FStatsMemoryDumpCommand::Run()
{
	FString SourceFilepath;
	FParse::Value( FCommandLine::Get(), TEXT( "-INFILE=" ), SourceFilepath );

	const FName NAME_ProfilerModule = TEXT( "Profiler" );
	IProfilerModule& ProfilerModule = FModuleManager::LoadModuleChecked<IProfilerModule>( NAME_ProfilerModule );
	ProfilerModule.StatsMemoryDumpCommand( *SourceFilepath );
}

