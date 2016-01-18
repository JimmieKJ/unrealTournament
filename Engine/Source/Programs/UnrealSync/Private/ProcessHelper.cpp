// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"

#include "ProcessHelper.h"

/**
 * Triggers progress event.
 *
 * @param Input Input string to pass.
 * @param OnProcessMadeProgress Progress delegate to pass input.
 *
 * @returns Passes down the result of OnUATMadeProgress which tells if process should continue.
 */
bool TriggerProgress(const FString& Input, const FOnProcessMadeProgress& OnProcessMadeProgress)
{
	if (OnProcessMadeProgress.IsBound())
	{
		return OnProcessMadeProgress.Execute(Input);
	}

	return true;
}

bool RunProcess(const FString& ExecutablePath, const FString& CommandLine)
{
	return RunProcessProgress(ExecutablePath, CommandLine, nullptr);
}

bool RunProcessOutput(const FString& ExecutablePath, const FString& CommandLine, FString& Output)
{
	class FOutputCollector
	{
	public:
		bool Progress(const FString& Chunk)
		{
			Output += Chunk;

			return true;
		}

		FString Output;
	};

	FOutputCollector OC;

	if (!RunProcessProgress(ExecutablePath, CommandLine, FOnProcessMadeProgress::CreateRaw(&OC, &FOutputCollector::Progress)))
	{
		return false;
	}

	Output = OC.Output;
	return true;
}

bool RunProcessProgress(const FString& ExecutablePath, const FString& CommandLine, const FOnProcessMadeProgress& OnProcessMadeProgress)
{
	if (!OnProcessMadeProgress.IsBound())
	{
		FPlatformProcess::CreateProc(*ExecutablePath, *CommandLine, true, false, false, NULL, 0, NULL, NULL);
		return true;
	}

	void* ReadPipe;
	void* WritePipe;

	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *CommandLine, false, true, true, NULL, 0, NULL, WritePipe);

	while (FPlatformProcess::IsProcRunning(ProcessHandle))
	{
		if (!TriggerProgress(FPlatformProcess::ReadPipe(ReadPipe), OnProcessMadeProgress))
		{
			FPlatformProcess::TerminateProc(ProcessHandle);
			FPlatformProcess::WaitForProc(ProcessHandle);
			return false;
		}
	}

	FPlatformProcess::WaitForProc(ProcessHandle);
	FString Buffer;
	while (!(Buffer = FPlatformProcess::ReadPipe(ReadPipe)).IsEmpty())
	{
		TriggerProgress(Buffer, OnProcessMadeProgress);
	}

	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	int32 ReturnCode;
	if (!FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode))
	{
		return false;
	}

	return ReturnCode == 0;
}
