// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ParallelExecutor.h"
#include "RequiredProgramMainCPPInclude.h"
#include "BuildGraph.h"

IMPLEMENT_APPLICATION(ParallelExecutor, "ParallelExecutor")


int main(int ArgC, const char* ArgV[])
{
	// TODO: command line options for limiting number of processes
	// TODO: add ability to modify environment for child process, and add to job
	// TODO: add a FPlatformMisc::WaitForProcessOutput() with optional parameter for cancel event

	FCommandLine::Set(TEXT(""));

	setvbuf(stdout, NULL, _IONBF, 0);

	if(ArgC != 2)
	{
		wprintf(TEXT("Missing argument to ParallelExecutor for build graph."));
		return 1;
	}

	FBuildGraph Graph;
	if(!Graph.ReadFromFile(ArgV[1]))
	{
		wprintf(TEXT("Couldn't read '%hs'"), ArgV[1]);
		return 1;
	}

	return Graph.ExecuteInParallel(FPlatformMisc::NumberOfCoresIncludingHyperthreads());
}
