// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Misc/MonitoredProcess.h"

/* FMonitoredProcess structors
 *****************************************************************************/

FMonitoredProcess::FMonitoredProcess( const FString& InURL, const FString& InParams, bool InHidden, bool InCreatePipes )
	: Canceling(false)
	, EndTime(0)
	, Hidden(InHidden)
	, KillTree(false)
	, Params(InParams)
	, ReadPipe(nullptr)
	, ReturnCode(0)
	, StartTime(0)
	, Thread(nullptr)
	, bIsRunning(false)
	, URL(InURL)
	, WritePipe(nullptr)
	, bCreatePipes(InCreatePipes)
	, SleepInterval(0.0f)
{ }


FMonitoredProcess::~FMonitoredProcess()
{
	if (IsRunning())
	{
		Cancel(true);
	}

	if (Thread != nullptr) 
	{
		Thread->WaitForCompletion();
		delete Thread;
	}
}


/* FMonitoredProcess interface
 *****************************************************************************/

FTimespan FMonitoredProcess::GetDuration() const
{
	if (IsRunning())
	{
		return (FDateTime::UtcNow() - StartTime);
	}

	return (EndTime - StartTime);
}


bool FMonitoredProcess::Launch()
{
	if (IsRunning())
	{
		return false;
	}

	check (Thread == nullptr); // We shouldn't be calling this twice

	if (bCreatePipes && !FPlatformProcess::CreatePipe(ReadPipe, WritePipe))
	{
		return false;
	}

	ProcessHandle = FPlatformProcess::CreateProc(*URL, *Params, false, Hidden, Hidden, nullptr, 0, *FPaths::RootDir(), WritePipe);

	if (!ProcessHandle.IsValid())
	{
		return false;
	}

	static int32 MonitoredProcessIndex = 0;
	const FString MonitoredProcessName = FString::Printf( TEXT( "FMonitoredProcess %d" ), MonitoredProcessIndex );
	MonitoredProcessIndex++;

	bIsRunning = true;
	Thread = FRunnableThread::Create(this, *MonitoredProcessName, 128 * 1024, TPri_AboveNormal);

	return true;
}


/* FMonitoredProcess implementation
 *****************************************************************************/

void FMonitoredProcess::ProcessOutput( const FString& Output )
{
	TArray<FString> LogLines;

	Output.ParseIntoArray(LogLines, TEXT("\n"), false);

	for (int32 LogIndex = 0; LogIndex < LogLines.Num(); ++LogIndex)
	{
		OutputDelegate.ExecuteIfBound(LogLines[LogIndex]);
	}
}


/* FRunnable interface
 *****************************************************************************/

uint32 FMonitoredProcess::Run()
{
	// monitor the process
	StartTime = FDateTime::UtcNow();
	{
		do
		{
			FPlatformProcess::Sleep(SleepInterval);

			ProcessOutput(FPlatformProcess::ReadPipe(ReadPipe));

			if (Canceling)
			{
				FPlatformProcess::TerminateProc(ProcessHandle, KillTree);
				CanceledDelegate.ExecuteIfBound();
				bIsRunning = false;

				return 0;
			}
		}
		while (FPlatformProcess::IsProcRunning(ProcessHandle));
	}
	EndTime = FDateTime::UtcNow();

	// close output pipes
	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
	ReadPipe = WritePipe = nullptr;

	// get completion status
	if (!FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode))
	{
		ReturnCode = -1;
	}

	CompletedDelegate.ExecuteIfBound(ReturnCode);
	bIsRunning = false;

	return 0;
}
