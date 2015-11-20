// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


// ShaderCompileWorker.cpp : Defines the entry point for the console application.
//

#include "Core.h"
#include "RequiredProgramMainCPPInclude.h"
#include "ShaderCore.h"
#include "ExceptionHandling.h"
#include "IShaderFormat.h"
#include "IShaderFormatModule.h"

#define DEBUG_USING_CONSOLE	0

// this is for the protocol, not the data, bump if FShaderCompilerInput or ProcessInputFromArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerInputVersion = 5;
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerOutputVersion = 3;
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerSingleJobHeader = 'S';
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerPipelineJobHeader = 'P';

double LastCompileTime = 0.0;

static bool GShaderCompileUseXGE = false;
static bool GFailedDueToShaderFormatVersion = false;

static void WriteXGESuccessFile(const TCHAR* WorkingDirectory)
{
	// To signal compilation completion, create a zero length file in the working directory.
	delete IFileManager::Get().CreateFileWriter(*FString::Printf(TEXT("%s/Success"), WorkingDirectory), FILEWRITE_EvenIfReadOnly);
}

static const TArray<const IShaderFormat*>& GetShaderFormats()
{
	static bool bInitialized = false;
	static TArray<const IShaderFormat*> Results;

	if (!bInitialized)
	{
		bInitialized = true;
		Results.Empty(Results.Num());

		TArray<FName> Modules;
		FModuleManager::Get().FindModules(SHADERFORMAT_MODULE_WILDCARD, Modules);

		if (!Modules.Num())
		{
			UE_LOG(LogShaders, Error, TEXT("No target shader formats found!"));
		}

		for (int32 Index = 0; Index < Modules.Num(); Index++)
		{
			IShaderFormat* Format = FModuleManager::LoadModuleChecked<IShaderFormatModule>(Modules[Index]).GetShaderFormat();
			if (Format != nullptr)
			{
				Results.Add(Format);
			}
		}
	}
	return Results;
}

static const IShaderFormat* FindShaderFormat(FName Name)
{
	const TArray<const IShaderFormat*>& ShaderFormats = GetShaderFormats();	

	for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
	{
		TArray<FName> Formats;
		ShaderFormats[Index]->GetSupportedFormats(Formats);
		for (int32 FormatIndex = 0; FormatIndex < Formats.Num(); FormatIndex++)
		{
			if (Formats[FormatIndex] == Name)
			{
				return ShaderFormats[Index];
			}
		}
	}

	return nullptr;
}
	
/** Processes a compilation job. */
static void ProcessCompilationJob(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory)
{
	const IShaderFormat* Compiler = FindShaderFormat(Input.ShaderFormat);
	if (!Compiler)
	{
		UE_LOG(LogShaders, Fatal, TEXT("Can't compile shaders for format %s"), *Input.ShaderFormat.ToString());
	}

	// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
	Compiler->CompileShader(Input.ShaderFormat, Input, Output, WorkingDirectory);
}


class FWorkLoop
{
public:
	enum ECommunicationMode
	{
		ThroughFile,
	};
	FWorkLoop(const TCHAR* ParentProcessIdText,const TCHAR* InWorkingDirectory,const TCHAR* InInputFilename,const TCHAR* InOutputFilename, ECommunicationMode InCommunicationMode, TMap<FString, uint16>& InFormatVersionMap)
	:	ParentProcessId(FCString::Atoi(ParentProcessIdText))
	,	WorkingDirectory(InWorkingDirectory)
	,	InputFilename(InInputFilename)
	,	OutputFilename(InOutputFilename)
	,	CommunicationMode(InCommunicationMode)
	,	InputFilePath(InCommunicationMode == ThroughFile ? (FString(InWorkingDirectory) + InInputFilename) : InInputFilename)
	,	OutputFilePath(InCommunicationMode == ThroughFile ? (FString(InWorkingDirectory) + InOutputFilename) : InOutputFilename)
	,	FormatVersionMap(InFormatVersionMap)
	{
	}

	void Loop()
	{
		UE_LOG(LogShaders, Log, TEXT("Entering job loop"));

		while(true)
		{
			TArray<FJobResult> SingleJobResults;
			TArray<FPipelineJobResult> PipelineJobResults;

			// Read & Process Input
			{
				FArchive* InputFilePtr = OpenInputFile();
				if(!InputFilePtr)
				{
					break;
				}

				UE_LOG(LogShaders, Log, TEXT("Processing shader"));
				LastCompileTime = FPlatformTime::Seconds();

				ProcessInputFromArchive(InputFilePtr, SingleJobResults, PipelineJobResults);

				// Close the input file.
				delete InputFilePtr;
			}

			// Prepare for output
			FArchive* OutputFilePtr = CreateOutputArchive();
			check(OutputFilePtr);
			WriteToOutputArchive(OutputFilePtr, SingleJobResults, PipelineJobResults);

			// Close the output file.
			delete OutputFilePtr;

			// Change the output file name to requested one
			IFileManager::Get().Move(*OutputFilePath, *TempFilePath);

			if (GShaderCompileUseXGE)
			{
				// To signal compilation completion, create a zero length file in the working directory.
				WriteXGESuccessFile(*WorkingDirectory);

				// We only do one pass per process when using XGE.
				break;
			}
		}

		UE_LOG(LogShaders, Log, TEXT("Exiting job loop"));
	}

private:
	struct FJobResult
	{
		FShaderCompilerOutput CompilerOutput;
	};

	struct FPipelineJobResult
	{
		FString PipelineName;
		TArray<FJobResult> SingleJobs;
	};

	const int32 ParentProcessId;
	const FString WorkingDirectory;
	const FString InputFilename;
	const FString OutputFilename;

	const ECommunicationMode CommunicationMode;

	const FString InputFilePath;
	const FString OutputFilePath;
	TMap<FString, uint16> FormatVersionMap;
	FString TempFilePath;

	/** Opens an input file, trying multiple times if necessary. */
	FArchive* OpenInputFile()
	{
		FArchive* InputFile = nullptr;
		bool bFirstOpenTry = true;
		while(!InputFile && !GIsRequestingExit)
		{
			// Try to open the input file that we are going to process
			if (CommunicationMode == ThroughFile)
			{
				InputFile = IFileManager::Get().CreateFileReader(*InputFilePath,FILEREAD_Silent);
			}

			if(!InputFile && !bFirstOpenTry)
			{
				CheckExitConditions();
				// Give up CPU time while we are waiting
				FPlatformProcess::Sleep(0.01f);
			}
			bFirstOpenTry = false;
		}
		return InputFile;
	}

	void VerifyFormatVersions(TMap<FString, uint16>& ReceivedFormatVersionMap)
	{
		for (auto Pair : ReceivedFormatVersionMap)
		{
			auto* Found = FormatVersionMap.Find(Pair.Key);
			if (Found)
			{
				GFailedDueToShaderFormatVersion = true;
				if (Pair.Value != *Found)
				{
					FCString::Snprintf(GErrorExceptionDescription, sizeof(GErrorExceptionDescription), TEXT("Mismatched shader version for format %s; did you forget to build ShaderCompilerWorker?"), *Pair.Key, *Found, Pair.Value);

					checkf(Pair.Value == *Found, TEXT("Exiting due to mismatched shader version for format %s, version %d from ShaderCompilerWorker, received %d! Did you forget to build ShaderCompilerWorker?"), *Pair.Key, *Found, Pair.Value);
				}
			}
		}
	}

	void ProcessInputFromArchive(FArchive* InputFilePtr, TArray<FJobResult>& OutSingleJobResults, TArray<FPipelineJobResult>& OutPipelineJobResults)
	{
		FArchive& InputFile = *InputFilePtr;
		int32 InputVersion;
		InputFile << InputVersion;
		checkf(ShaderCompileWorkerInputVersion == InputVersion, TEXT("Exiting due to ShaderCompilerWorker expecting input version %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerInputVersion, InputVersion);

		TMap<FString, uint16> ReceivedFormatVersionMap;
		InputFile << ReceivedFormatVersionMap;

		VerifyFormatVersions(ReceivedFormatVersionMap);

		// Individual jobs
		{
			int32 SingleJobHeader = ShaderCompileWorkerSingleJobHeader;
			InputFile << SingleJobHeader;
			checkf(ShaderCompileWorkerSingleJobHeader == SingleJobHeader, TEXT("Exiting due to ShaderCompilerWorker expecting job header %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerSingleJobHeader, SingleJobHeader);

			int32 NumBatches = 0;
			InputFile << NumBatches;

			// Flush cache, to make sure we load the latest version of the input file.
			// (Otherwise quick changes to a shader file can result in the wrong output.)
			FlushShaderFileCache();

			for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
			{
				// Deserialize the job's inputs.
				FShaderCompilerInput CompilerInput;
				InputFile << CompilerInput;

				if (IsValidRef(CompilerInput.SharedEnvironment))
				{
					// Merge the shared environment into the per-shader environment before calling into the compile function
					CompilerInput.Environment.Merge(*CompilerInput.SharedEnvironment);
				}

				// Process the job.
				FShaderCompilerOutput CompilerOutput;
				ProcessCompilationJob(CompilerInput, CompilerOutput, WorkingDirectory);

				// Serialize the job's output.
				FJobResult& JobResult = *new(OutSingleJobResults) FJobResult;
				JobResult.CompilerOutput = CompilerOutput;
			}
		}

		// Shader pipeline jobs
		{
			int32 PipelineJobHeader = ShaderCompileWorkerPipelineJobHeader;
			InputFile << PipelineJobHeader;
			checkf(ShaderCompileWorkerPipelineJobHeader == PipelineJobHeader, TEXT("Exiting due to ShaderCompilerWorker expecting pipeline job header %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerSingleJobHeader, PipelineJobHeader);

			int32 NumPipelines = 0;
			InputFile << NumPipelines;

			for (int32 Index = 0; Index < NumPipelines; ++Index)
			{
				FPipelineJobResult& PipelineJob = *new(OutPipelineJobResults) FPipelineJobResult;

				InputFile << PipelineJob.PipelineName;

				int32 NumStages = 0;
				InputFile << NumStages;

				TArray<FShaderCompilerInput> CompilerInputs;
				CompilerInputs.AddDefaulted(NumStages);

				for (int32 StageIndex = 0; StageIndex < NumStages; ++StageIndex)
				{
					// Deserialize the job's inputs.
					InputFile << CompilerInputs[StageIndex];

					if (IsValidRef(CompilerInputs[StageIndex].SharedEnvironment))
					{
						// Merge the shared environment into the per-shader environment before calling into the compile function
						CompilerInputs[StageIndex].Environment.Merge(*CompilerInputs[StageIndex].SharedEnvironment);
					}
				}

				ProcessShaderPipelineCompilationJob(PipelineJob, CompilerInputs);
			}
		}
	}

	void ProcessShaderPipelineCompilationJob(FPipelineJobResult& PipelineJob, TArray<FShaderCompilerInput>& CompilerInputs)
	{
		checkf(CompilerInputs.Num() > 0, TEXT("Exiting due to Pipeline %s having zero jobs!"), *PipelineJob.PipelineName);

		// Process the job.
		FShaderCompilerOutput FirstCompilerOutput;
		CompilerInputs[0].bCompilingForShaderPipeline = true;
		CompilerInputs[0].bIncludeUsedOutputs = false;
		ProcessCompilationJob(CompilerInputs[0], FirstCompilerOutput, WorkingDirectory);

		// Serialize the job's output.
		{
			FJobResult& JobResult = *new(PipelineJob.SingleJobs) FJobResult;
			JobResult.CompilerOutput = FirstCompilerOutput;
		}

		bool bEnableRemovingUnused = true;

		//#todo-rco: Only remove for pure VS & PS stages
		for (int32 Index = 0; Index < CompilerInputs.Num(); ++Index)
		{
			auto Stage = CompilerInputs[Index].Target.Frequency;
			if (Stage != SF_Vertex && Stage != SF_Pixel)
			{
				bEnableRemovingUnused = false;
				break;
			}
		}

		for (int32 Index = 1; Index < CompilerInputs.Num(); ++Index)
		{
			if (bEnableRemovingUnused && PipelineJob.SingleJobs.Last().CompilerOutput.bSupportsQueryingUsedAttributes)
			{
				CompilerInputs[Index].bIncludeUsedOutputs = true;
				CompilerInputs[Index].bCompilingForShaderPipeline = true;
				CompilerInputs[Index].UsedOutputs = PipelineJob.SingleJobs.Last().CompilerOutput.UsedAttributes;
			}

			FShaderCompilerOutput CompilerOutput;
			ProcessCompilationJob(CompilerInputs[Index], CompilerOutput, WorkingDirectory);

			// Serialize the job's output.
			FJobResult& JobResult = *new(PipelineJob.SingleJobs) FJobResult;
			JobResult.CompilerOutput = CompilerOutput;
		}
	}

	FArchive* CreateOutputArchive()
	{
		FArchive* OutputFilePtr = nullptr;
		if (CommunicationMode == ThroughFile)
		{
			const double StartTime = FPlatformTime::Seconds();
			bool bResult = false;

			// It seems XGE does not support deleting files.
			// Don't delete the input file if we are running under Incredibuild.
			// Instead, we signal completion by creating a zero byte "Success" file after the output file has been fully written.
			if (!GShaderCompileUseXGE)
			{
				do 
				{
					// Remove the input file so that it won't get processed more than once
					bResult = IFileManager::Get().Delete(*InputFilePath);
				} 
				while (!bResult && (FPlatformTime::Seconds() - StartTime < 2));

				if (!bResult)
				{
					UE_LOG(LogShaders, Fatal,TEXT("Couldn't delete input file %s, is it readonly?"), *InputFilePath);
				}
			}

			// To make sure that the process waiting for results won't read unfinished output file,
			// we use a temp file name during compilation.
			do
			{
				FGuid Guid;
				FPlatformMisc::CreateGuid(Guid);
				TempFilePath = WorkingDirectory + Guid.ToString();
			} while (IFileManager::Get().FileSize(*TempFilePath) != INDEX_NONE);

			const double StartTime2 = FPlatformTime::Seconds();

			do 
			{
				// Create the output file.
				OutputFilePtr = IFileManager::Get().CreateFileWriter(*TempFilePath,FILEWRITE_EvenIfReadOnly);
			} 
			while (!OutputFilePtr && (FPlatformTime::Seconds() - StartTime2 < 2));
			
			if (!OutputFilePtr)
			{
				UE_LOG(LogShaders, Fatal,TEXT("Couldn't save output file %s"), *TempFilePath);
			}
		}

		return OutputFilePtr;
	}

	void WriteToOutputArchive(FArchive* OutputFilePtr, TArray<FJobResult>& SingleJobResults, TArray<FPipelineJobResult>& PipelineJobResults)
	{
		FArchive& OutputFile = *OutputFilePtr;

		int32 OutputVersion = ShaderCompileWorkerOutputVersion;
		OutputFile << OutputVersion;

		int32 ErrorCode = 0;
		OutputFile << ErrorCode;

		int32 ErrorStringLength = 0;
		OutputFile << ErrorStringLength;
		OutputFile << ErrorStringLength;

		{
			int32 SingleJobHeader = ShaderCompileWorkerSingleJobHeader;
			OutputFile << SingleJobHeader;

			int32 NumBatches = SingleJobResults.Num();
			OutputFile << NumBatches;

			for (int32 ResultIndex = 0; ResultIndex < SingleJobResults.Num(); ResultIndex++)
			{
				FJobResult& JobResult = SingleJobResults[ResultIndex];
				OutputFile << JobResult.CompilerOutput;
			}
		}

		{
			int32 PipelineJobHeader = ShaderCompileWorkerPipelineJobHeader;
			OutputFile << PipelineJobHeader;
			int32 NumBatches = PipelineJobResults.Num();
			OutputFile << NumBatches;

			for (int32 ResultIndex = 0; ResultIndex < PipelineJobResults.Num(); ResultIndex++)
			{
				auto& PipelineJob = PipelineJobResults[ResultIndex];
				OutputFile << PipelineJob.PipelineName;
				int32 NumStageJobs = PipelineJob.SingleJobs.Num();
				OutputFile << NumStageJobs;
				for (int32 Index = 0; Index < NumStageJobs; ++Index)
				{
					FJobResult& JobResult = PipelineJob.SingleJobs[Index];
					OutputFile << JobResult.CompilerOutput;
				}
			}
		}
	}

	/** Called in the idle loop, checks for conditions under which the helper should exit */
	void CheckExitConditions()
	{
		if (!InputFilename.Contains(TEXT("Only")))
		{
			UE_LOG(LogShaders, Log, TEXT("InputFilename did not contain 'Only', exiting after one job."));
			FPlatformMisc::RequestExit(false);
		}

#if PLATFORM_MAC || PLATFORM_LINUX
		if (!FPlatformMisc::IsDebuggerPresent() && ParentProcessId > 0)
		{
			// If the parent process is no longer running, exit
			if (!FPlatformProcess::IsApplicationRunning(ParentProcessId))
			{
				FString FilePath = FString(WorkingDirectory) + InputFilename;
				checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to the parent process no longer running and the input file is present!"));
				UE_LOG(LogShaders, Log, TEXT("Parent process no longer running, exiting"));
				FPlatformMisc::RequestExit(false);
			}
		}

		const double CurrentTime = FPlatformTime::Seconds();
		// If we have been idle for 20 seconds then exit
		if (CurrentTime - LastCompileTime > 20.0)
		{
			UE_LOG(LogShaders, Log, TEXT("No jobs found for 20 seconds, exiting"));
			FPlatformMisc::RequestExit(false);
		}
#else
		// Don't do these if the debugger is present
		//@todo - don't do these if Unreal is being debugged either
		if (!IsDebuggerPresent())
		{
			if (ParentProcessId > 0)
			{
				FString FilePath = FString(WorkingDirectory) + InputFilename;

				bool bParentStillRunning = true;
				HANDLE ParentProcessHandle = OpenProcess(SYNCHRONIZE, false, ParentProcessId);
				// If we couldn't open the process then it is no longer running, exit
				if (ParentProcessHandle == nullptr)
				{
					checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to OpenProcess(ParentProcessId) failing and the input file is present!"));
					UE_LOG(LogShaders, Log, TEXT("Couldn't OpenProcess, Parent process no longer running, exiting"));
					FPlatformMisc::RequestExit(false);
				}
				else
				{
					// If we did open the process, that doesn't mean it is still running
					// The process object stays alive as long as there are handles to it
					// We need to check if the process has signaled, which indicates that it has exited
					uint32 WaitResult = WaitForSingleObject(ParentProcessHandle, 0);
					if (WaitResult != WAIT_TIMEOUT)
					{
						checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to WaitForSingleObject(ParentProcessHandle) signaling and the input file is present!"));
						UE_LOG(LogShaders, Log, TEXT("WaitForSingleObject signaled, Parent process no longer running, exiting"));
						FPlatformMisc::RequestExit(false);
					}
					CloseHandle(ParentProcessHandle);
				}
			}

			const double CurrentTime = FPlatformTime::Seconds();
			// If we have been idle for 20 seconds then exit
			if (CurrentTime - LastCompileTime > 20.0)
			{
				UE_LOG(LogShaders, Log, TEXT("No jobs found for 20 seconds, exiting"));
				FPlatformMisc::RequestExit(false);
			}
		}
#endif
	}
};

/** 
 * Main entrypoint, guarded by a try ... except.
 * This expects 4 parameters:
 *		The image path and name
 *		The working directory path, which has to be unique to the instigating process and thread.
 *		The parent process Id
 *		The thread Id corresponding to this worker
 */
int32 GuardedMain(int32 argc, TCHAR* argv[])
{
	GEngineLoop.PreInit(argc, argv, TEXT("-NOPACKAGECACHE -Multiprocess"));
#if DEBUG_USING_CONSOLE
	GLogConsole->Show( true );
#endif

#if PLATFORM_WINDOWS
	//@todo - would be nice to change application name or description to have the ThreadId in it for debugging purposes
	SetConsoleTitle(argv[3]);
#endif

	// We just enumerate the shader formats here for debugging.
	const TArray<const class IShaderFormat*>& ShaderFormats = GetShaderFormats();
	check(ShaderFormats.Num());
	TMap<FString, uint16> FormatVersionMap;
	for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
	{
		TArray<FName> OutFormats;
		ShaderFormats[Index]->GetSupportedFormats(OutFormats);
		check(OutFormats.Num());
		for (int32 InnerIndex = 0; InnerIndex < OutFormats.Num(); InnerIndex++)
		{
			UE_LOG(LogShaders, Display, TEXT("Available Shader Format %s"), *OutFormats[InnerIndex].ToString());
			uint16 Version = ShaderFormats[Index]->GetVersion(OutFormats[InnerIndex]);
			FormatVersionMap.Add(OutFormats[InnerIndex].ToString(), Version);
		}
	}

	LastCompileTime = FPlatformTime::Seconds();

	FWorkLoop::ECommunicationMode Mode = FWorkLoop::ThroughFile;
	FWorkLoop WorkLoop(argv[2], argv[1], argv[4], argv[5], Mode, FormatVersionMap);

	WorkLoop.Loop();

	return 0;
}

int32 GuardedMainWrapper(int32 ArgC, TCHAR* ArgV[], const TCHAR* CrashOutputFile)
{
	// We need to know whether we are using XGE now, in case an exception
	// is thrown before we parse the command line inside GuardedMain.
	GShaderCompileUseXGE = (ArgC > 6) && FCString::Strcmp(ArgV[6], TEXT("-xge")) == 0;

	int32 ReturnCode = 0;
#if PLATFORM_WINDOWS
	if (FPlatformMisc::IsDebuggerPresent())
#endif
	{
		ReturnCode = GuardedMain(ArgC, ArgV);
	}
#if PLATFORM_WINDOWS
	else
	{
		// Don't want 32 dialogs popping up when SCW fails
		GUseCrashReportClient = false;
		__try
		{
			GIsGuarded = 1;
			ReturnCode = GuardedMain(ArgC, ArgV);
			GIsGuarded = 0;
		}
		__except( ReportCrash( GetExceptionInformation() ) )
		{
			FArchive& OutputFile = *IFileManager::Get().CreateFileWriter(CrashOutputFile,FILEWRITE_NoFail);

			int32 OutputVersion = ShaderCompileWorkerOutputVersion;
			OutputFile << OutputVersion;

			int32 ErrorCode = GFailedDueToShaderFormatVersion ? 2 : 1;
			OutputFile << ErrorCode;

			// Note: Can't use FStrings here as SEH can't be used with destructors
			int32 CallstackLength = FCString::Strlen(GErrorHist);
			OutputFile << CallstackLength;

			int32 ExceptionInfoLength = FCString::Strlen(GErrorExceptionDescription);
			OutputFile << ExceptionInfoLength;

			OutputFile.Serialize(GErrorHist, CallstackLength * sizeof(TCHAR));
			OutputFile.Serialize(GErrorExceptionDescription, ExceptionInfoLength * sizeof(TCHAR));

			int32 NumBatches = 0;
			OutputFile << NumBatches;

			// Close the output file.
			delete &OutputFile;

			if (GShaderCompileUseXGE)
			{
				ReturnCode = 1;
				WriteXGESuccessFile(ArgV[1]);
			}
		}
	}
#endif

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	return ReturnCode;
}

IMPLEMENT_APPLICATION(ShaderCompileWorker, "ShaderCompileWorker")


/**
 * Application entry point
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	// FPlatformProcess::OpenProcess only implemented for windows atm
#if PLATFORM_WINDOWS
	if (ArgC == 4 && FCString::Strcmp(ArgV[1], TEXT("-xgemonitor")) == 0)
	{
		// Open handles to the two processes
		FProcHandle EngineProc = FPlatformProcess::OpenProcess(FCString::Atoi(ArgV[2]));
		FProcHandle BuildProc = FPlatformProcess::OpenProcess(FCString::Atoi(ArgV[3]));

		if (EngineProc.IsValid() && BuildProc.IsValid())
		{
			// Whilst the build is still in progress
			while (FPlatformProcess::IsProcRunning(BuildProc))
			{
				// Check that the engine is still alive.
				if (!FPlatformProcess::IsProcRunning(EngineProc))
				{
					// The engine has shutdown before the build was stopped.
					// Kill off the build process
					FPlatformProcess::TerminateProc(BuildProc);
					break;
				}

				FPlatformProcess::Sleep(0.01f);
			}
		}
		return 0;
	}
#endif
	if(ArgC < 6)
	{
		printf("ShaderCompileWorker is called by UE4, it requires specific command like arguments.\n");
		return -1;
	}

	// Game exe can pass any number of parameters through with appGetSubprocessCommandline
	// so just make sure we have at least the minimum number of parameters.
	check(ArgC >= 6);

	TCHAR OutputFilePath[PLATFORM_MAX_FILEPATH_LENGTH];
	FCString::Strncpy(OutputFilePath, ArgV[1], PLATFORM_MAX_FILEPATH_LENGTH);
	FCString::Strncat(OutputFilePath, ArgV[5], PLATFORM_MAX_FILEPATH_LENGTH);

	const int32 ReturnCode = GuardedMainWrapper(ArgC,ArgV,OutputFilePath);
	return ReturnCode;
}
