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

const int32 ShaderCompileWorkerInputVersion = 2;
const int32 ShaderCompileWorkerOutputVersion = 1;

double LastCompileTime = 0.0;

static bool GShaderCompileUseXGE = false;
static void WriteXGESuccessFile(const TCHAR* WorkingDirectory)
{
	// To signal compilation completion, create a zero length file in the working directory.
	delete IFileManager::Get().CreateFileWriter(*FString::Printf(TEXT("%s/Success"), WorkingDirectory), FILEWRITE_EvenIfReadOnly);
}

const TArray<const IShaderFormat*>& GetShaderFormats()
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

const IShaderFormat* FindShaderFormat(FName Name)
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

#if PLATFORM_SUPPORTS_NAMED_PIPES
static void VerifyResult(bool bResult, const TCHAR* InMessage = TEXT(""))
{
#if PLATFORM_WINDOWS
	if (!bResult)
	{
		TCHAR ErrorBuffer[ MAX_SPRINTF ];
		uint32 Error = GetLastError();
		const TCHAR* Buffer = FWindowsPlatformMisc::GetSystemErrorMessage(ErrorBuffer, MAX_SPRINTF, Error);
		FString Message = FString::Printf(TEXT("FAILED (%s) with GetLastError() %d: %s!\n"), InMessage, Error, ErrorBuffer);
		FPlatformMisc::LowLevelOutputDebugStringf(*Message);
		UE_LOG(LogShaders, Fatal, TEXT("%s"), *Message);
	}
#endif // PLATFORM_WINDOWS
	verify(bResult != 0);
}
#endif

class FWorkLoop
{
public:
	enum ECommunicationMode
	{
		ThroughFile,
		ThroughNamedPipeOnce,
		ThroughNamedPipe,
	};
	FWorkLoop(const TCHAR* ParentProcessIdText,const TCHAR* InWorkingDirectory,const TCHAR* InInputFilename,const TCHAR* InOutputFilename, ECommunicationMode InCommunicationMode)
	:	ParentProcessId(FCString::Atoi(ParentProcessIdText))
	,	WorkingDirectory(InWorkingDirectory)
	,	InputFilename(InInputFilename)
	,	OutputFilename(InOutputFilename)
	,	CommunicationMode(InCommunicationMode)
	,	InputFilePath(InCommunicationMode == ThroughFile ? (FString(InWorkingDirectory) + InInputFilename) : InInputFilename)
	,	OutputFilePath(InCommunicationMode == ThroughFile ? (FString(InWorkingDirectory) + InOutputFilename) : InOutputFilename)
	{
#if PLATFORM_SUPPORTS_NAMED_PIPES
		LastConnectionTime = FPlatformTime::Seconds();
#endif
	}

	void Loop()
	{
		UE_LOG(LogShaders, Log, TEXT("Entering job loop"));

		while(true)
		{
			TArray<FJobResult> JobResults;

			// Read & Process Input
			{
				FArchive* InputFilePtr = OpenInputFile();
				if(!InputFilePtr)
				{
					break;
				}

				UE_LOG(LogShaders, Log, TEXT("Processing shader"));
				LastCompileTime = FPlatformTime::Seconds();

				ProcessInputFromArchive(InputFilePtr, JobResults);

				// Close the input file.
				delete InputFilePtr;
			}

			// Prepare for output
			FArchive* OutputFilePtr = CreateOutputArchive();
			check(OutputFilePtr);
			WriteToOutputArchive(OutputFilePtr, JobResults);

			// Close the output file.
			delete OutputFilePtr;

			// Change the output file name to requested one
			IFileManager::Get().Move(*OutputFilePath, *TempFilePath);

#if PLATFORM_SUPPORTS_NAMED_PIPES
			if (CommunicationMode == ThroughNamedPipeOnce || CommunicationMode == ThroughNamedPipe)
			{
				VerifyResult(Pipe.WriteInt32(TransferBufferOut.Num()), TEXT("Writing Transfer Size"));
				VerifyResult(Pipe.WriteBytes(TransferBufferOut.Num(), TransferBufferOut.GetData()), TEXT("Writing Transfer Buffer"));
//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** Closing pipe...\n"));
				Pipe.Destroy();

				if (CommunicationMode == ThroughNamedPipeOnce)
				{
					// Give up CPU time while we are waiting
					FPlatformProcess::Sleep(0.02f);
					break;
				}

				LastConnectionTime = FPlatformTime::Seconds();
			}
#endif	// PLATFORM_SUPPORTS_NAMED_PIPES

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

	const int32 ParentProcessId;
	const FString WorkingDirectory;
	const FString InputFilename;
	const FString OutputFilename;

	const ECommunicationMode CommunicationMode;

	const FString InputFilePath;
	const FString OutputFilePath;
	FString TempFilePath;

#if PLATFORM_SUPPORTS_NAMED_PIPES
	FPlatformNamedPipe Pipe;
	TArray<uint8> TransferBufferIn;
	TArray<uint8> TransferBufferOut;
	double LastConnectionTime;
#endif	// PLATFORM_SUPPORTS_NAMED_PIPES

	bool IsUsingNamedPipes() const
	{
		return (CommunicationMode == ThroughNamedPipeOnce || CommunicationMode == ThroughNamedPipe);
	}

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
			else
			{
#if PLATFORM_SUPPORTS_NAMED_PIPES
				check(IsUsingNamedPipes()); //UE_LOG(LogShaders, Log, TEXT("Opening Pipe %s\n"), *InputFilePath);
//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** Trying to open pipe %s\n"), *InputFilePath);
				if (Pipe.Create(InputFilePath, false, false))
				{
//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\tOpened!!!\n"));
					// Read the total number of bytes
					int32 TransferSize = 0;
					VerifyResult(Pipe.ReadInt32(TransferSize));

					// Prealloc and read the full buffer
					TransferBufferIn.Empty(TransferSize);
					TransferBufferIn.AddUninitialized(TransferSize);	//UE_LOG(LogShaders, Log, TEXT("Reading Buffer\n"));
					VerifyResult(Pipe.ReadBytes(TransferSize, TransferBufferIn.GetData()));

					return new FMemoryReader(TransferBufferIn);
				}

				double DeltaTime = FPlatformTime::Seconds();
				if (DeltaTime - LastConnectionTime > 20.0f)
				{
					// If can't connect for more than 20 seconds, let's exit
					FPlatformMisc::RequestExit(false);
				}
#endif	// PLATFORM_SUPPORTS_NAMED_PIPES
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

	void ProcessInputFromArchive(FArchive* InputFilePtr, TArray<FJobResult>& OutJobResults)
	{
		int32 NumBatches = 0;

		FArchive& InputFile = *InputFilePtr;
		int32 InputVersion;
		InputFile << InputVersion;
		check(ShaderCompileWorkerInputVersion == InputVersion);

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
			ProcessCompilationJob(CompilerInput,CompilerOutput,WorkingDirectory);

			// Serialize the job's output.
			FJobResult& JobResult = *new(OutJobResults) FJobResult;
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
		else
		{
#if PLATFORM_SUPPORTS_NAMED_PIPES
			check(IsUsingNamedPipes());

			// Output Transfer Buffer...
			TransferBufferOut.Empty(0);
			OutputFilePtr = new FMemoryWriter(TransferBufferOut);
#endif
		}

		return OutputFilePtr;
	}

	void WriteToOutputArchive(FArchive* OutputFilePtr, TArray<FJobResult>& JobResults)
	{
		FArchive& OutputFile = *OutputFilePtr;

		int32 OutputVersion = ShaderCompileWorkerOutputVersion;
		OutputFile << OutputVersion;

		int32 ErrorCode = 0;
		OutputFile << ErrorCode;

		int32 ErrorStringLength = 0;
		OutputFile << ErrorStringLength;
		OutputFile << ErrorStringLength;

		int32 NumBatches = JobResults.Num();
		OutputFile << NumBatches;

		for (int32 ResultIndex = 0; ResultIndex < JobResults.Num(); ResultIndex++)
		{
			FJobResult& JobResult = JobResults[ResultIndex];
			OutputFile << JobResult.CompilerOutput;
		}
	}

	/** Called in the idle loop, checks for conditions under which the helper should exit */
	void CheckExitConditions()
	{
#if PLATFORM_SUPPORTS_NAMED_PIPES
		if (CommunicationMode == ThroughNamedPipeOnce)
		{
			UE_LOG(LogShaders, Log, TEXT("PipeOnce: exiting after one job."));
			FPlatformMisc::RequestExit(false);
		}
		else if (CommunicationMode == ThroughFile)
#endif
		{
			if (!InputFilename.Contains(TEXT("Only")))
			{
				UE_LOG(LogShaders, Log, TEXT("InputFilename did not contain 'Only', exiting after one job."));
				FPlatformMisc::RequestExit(false);
			}
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
					if (!IsUsingNamedPipes())
					{
						checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to OpenProcess(ParentProcessId) failing and the input file is present!"));
					}
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
						if (!IsUsingNamedPipes())
						{
							checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to WaitForSingleObject(ParentProcessHandle) signaling and the input file is present!"));
						}
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
	for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
	{
		TArray<FName> OutFormats;
		ShaderFormats[Index]->GetSupportedFormats(OutFormats);
		check(OutFormats.Num());
		for (int32 InnerIndex = 0; InnerIndex < OutFormats.Num(); InnerIndex++)
		{
			UE_LOG(LogShaders, Display, TEXT("Available Shader Format %s"), *OutFormats[InnerIndex].ToString());
		}
	}

	LastCompileTime = FPlatformTime::Seconds();

	FString InCommunicating = (argc > 6) ? argv[6] : FString();
#if PLATFORM_SUPPORTS_NAMED_PIPES
	const bool bThroughFile = GShaderCompileUseXGE || (InCommunicating == FString(TEXT("-communicatethroughfile")));
	const bool bThroughNamedPipe = (InCommunicating == FString(TEXT("-communicatethroughnamedpipe")));
	const bool bThroughNamedPipeOnce = (InCommunicating == FString(TEXT("-communicatethroughnamedpipeonce")));
#else
	const bool bThroughFile = true;
	const bool bThroughNamedPipe = false;
	const bool bThroughNamedPipeOnce = false;
#endif
	check((int32)bThroughFile + (int32)bThroughNamedPipe + (int32)bThroughNamedPipeOnce == 1);

	FWorkLoop::ECommunicationMode Mode = bThroughFile ? FWorkLoop::ThroughFile : (bThroughNamedPipeOnce ? FWorkLoop::ThroughNamedPipeOnce : FWorkLoop::ThroughNamedPipe);
	FWorkLoop WorkLoop(argv[2], argv[1], argv[4], argv[5], Mode);

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

			int32 ErrorCode = 1;
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
