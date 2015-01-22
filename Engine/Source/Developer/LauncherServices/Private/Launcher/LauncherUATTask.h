// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlatformInfo.h"


/**
 * class for UAT launcher tasks.
 */
static const FString ConfigStrings[] = { TEXT("Unknown"), TEXT("Debug"), TEXT("DebugGame"), TEXT("Development"), TEXT("Shipping"), TEXT("Test") };

class FLauncherUATTask
	: public FLauncherTask
{
public:

	FLauncherUATTask( const FString& InCommandLine, const FString& InName, const FString& InDesc, void* InReadPipe, void* InWritePipe, const FString& InEditorExe, FProcHandle& InProcessHandle, ILauncherWorker* InWorker, const FString& InCommandEnd)
		: FLauncherTask(InName, InDesc, InReadPipe, InWritePipe)
		, CommandLine(InCommandLine)
		, EditorExe(InEditorExe)
		, ProcessHandle(InProcessHandle)
		, CommandText(InCommandEnd)
	{
		NoCompile = TEXT(" -nocompile");
		EndTextFound = false;
		InWorker->OnOutputReceived().AddRaw(this, &FLauncherUATTask::HandleOutputReceived);
	}

	static bool FirstTimeCompile;
protected:

	/**
	 * Performs the actual task.
	 *
	 * @param ChainState - Holds the state of the task chain.
	 *
	 * @return true if the task completed successfully, false otherwise.
	 */
	virtual bool PerformTask( FLauncherTaskChainState& ChainState ) override
	{
		if (FirstTimeCompile)
		{
			NoCompile = (!FParse::Param( FCommandLine::Get(), TEXT("development") ) && !ChainState.Profile->IsBuildingUAT()) ? TEXT(" -nocompile") : TEXT("");
			FirstTimeCompile = false;
		}

		// spawn a UAT process to cook the data
		// UAT executable
		FString ExecutablePath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() + FString(TEXT("Build")) / TEXT("BatchFiles"));
#if PLATFORM_MAC
		FString Executable = TEXT("RunUAT.command");
#elif PLATFORM_LINUX
		FString Executable = TEXT("RunUAT.sh");
#else
		FString Executable = TEXT("RunUAT.bat");
#endif

        // check for rocket
        FString Rocket;
        if ( FRocketSupport::IsRocket() )
        {
            Rocket = TEXT(" -rocket" );
        }
        
		// base UAT command arguments
		FString UATCommandLine;
		FString ProjectPath = *ChainState.Profile->GetProjectPath();
		ProjectPath = FPaths::ConvertRelativePathToFull(ProjectPath);
		UATCommandLine = FString::Printf(TEXT("BuildCookRun -project=\"%s\" -noP4 -clientconfig=%s -serverconfig=%s"),
			*ProjectPath,
			*ConfigStrings[ChainState.Profile->GetBuildConfiguration()],
			*ConfigStrings[ChainState.Profile->GetBuildConfiguration()]);

		UATCommandLine += NoCompile;
        UATCommandLine += Rocket;

		// specify the path to the editor exe if necessary
		if(EditorExe.Len() > 0)
		{
			UATCommandLine += FString::Printf(TEXT(" -ue4exe=\"%s\""), *EditorExe);
		}

		// specialized command arguments for this particular task
		UATCommandLine += CommandLine;

		// launch UAT and monitor its progress
		ProcessHandle = FPlatformProcess::CreateProc(*(ExecutablePath / Executable), *UATCommandLine, false, true, true, NULL, 0, *ExecutablePath, WritePipe);

		while (FPlatformProcess::IsProcRunning(ProcessHandle) && !EndTextFound)
		{

			if (GetStatus() == ELauncherTaskStatus::Canceling)
			{
				FPlatformProcess::TerminateProc(ProcessHandle, true);
				return false;
			}

			FPlatformProcess::Sleep(0.25);
		}

		if (!EndTextFound && !FPlatformProcess::GetProcReturnCode(ProcessHandle, &Result))
		{
			return false;
		}

		return (Result == 0);
	}

	void HandleOutputReceived(const FString& InMessage)
	{
		EndTextFound |= InMessage.Contains(CommandText);
	}

private:

	// Command line
	FString CommandLine;

	// Holds the no compile flag
	FString NoCompile;

	// The editor executable that UAT should use
	FString EditorExe;

	// process handle
	FProcHandle& ProcessHandle;

	// The editor executable that UAT should use
	FString CommandText;

	bool EndTextFound;
};

