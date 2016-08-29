// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PrivatePCH.h"

#include "TestDirectoryWatcher.h"
#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY(LogTestPAL);

IMPLEMENT_APPLICATION(TestPAL, "TestPAL");

#define ARG_PROC_TEST						"proc"
#define ARG_PROC_TEST_CHILD					"proc-child"
#define ARG_CASE_SENSITIVITY_TEST			"case"
#define ARG_MESSAGEBOX_TEST					"messagebox"
#define ARG_DIRECTORY_WATCHER_TEST			"dirwatcher"
#define ARG_THREAD_SINGLETON_TEST			"threadsingleton"
#define ARG_SYSINFO_TEST					"sysinfo"
#define ARG_CRASH_TEST						"crash"
#define ARG_STRINGPRECISION_TEST			"stringprecision"
#define ARG_DSO_TEST						"dso"

namespace TestPAL
{
	FString CommandLine;
};

/**
 * FProcHandle test (child instance)
 */
int32 ProcRunAsChild(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);

	// set a random delay pretending to do some useful work up to a minute. 
	srand(FPlatformProcess::GetCurrentProcessId());
	double RandomWorkTime = FMath::FRandRange(0.0f, 6.0f);

	UE_LOG(LogTestPAL, Display, TEXT("Running proc test as child (pid %d), will be doing work for %f seconds."), FPlatformProcess::GetCurrentProcessId(), RandomWorkTime);

	double StartTime = FPlatformTime::Seconds();

	// Use all the CPU!
	for (;;)
	{
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - StartTime >= RandomWorkTime)
		{
			break;
		}
	}

	UE_LOG(LogTestPAL, Display, TEXT("Child (pid %d) finished work."), FPlatformProcess::GetCurrentProcessId());

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * FProcHandle test (parent instance)
 */
int32 ProcRunAsParent(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running proc test as parent."));

	// Run slave instance continuously
	int NumChildrenToSpawn = 255, MaxAtOnce = 5;
	FParent Parent(NumChildrenToSpawn, MaxAtOnce);

	Parent.Run();

	UE_LOG(LogTestPAL, Display, TEXT("Parent quit."));

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Tests a single file.
 */
void TestCaseInsensitiveFile(const FString & Filename, const FString & WrongFilename)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	IFileHandle * CreationHandle = PlatformFile.OpenWrite(*Filename);
	checkf(CreationHandle, TEXT("Could not create a test file for '%s'"), *Filename);
	delete CreationHandle;
	
	IFileHandle* CheckGoodHandle = PlatformFile.OpenRead(*Filename);
	checkf(CheckGoodHandle, TEXT("Could not open a test file for '%s' (zero probe)"), *Filename);
	delete CheckGoodHandle;
	
	IFileHandle* CheckWrongCaseRelHandle = PlatformFile.OpenRead(*WrongFilename);
	checkf(CheckWrongCaseRelHandle, TEXT("Could not open a test file for '%s'"), *WrongFilename);
	delete CheckWrongCaseRelHandle;
	
	PlatformFile.DeleteFile(*Filename);
}

/**
 * Case-(in)sensitivity test/
 */
int32 CaseTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();
	
	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running case sensitivity test."));
	
	TestCaseInsensitiveFile(TEXT("Test.Test"), TEXT("teSt.teSt"));
	
	FString File(TEXT("Test^%!CaseInsens"));
	FString AbsFile = FPaths::ConvertRelativePathToFull(File);
	FString AbsFileUpper = AbsFile.ToUpper();

	TestCaseInsensitiveFile(AbsFile, AbsFileUpper);
	
	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Message box test/
 */
int32 MessageBoxTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running message box test."));
	
	FString Display(TEXT("I am a big big string in a big big game, it's not a big big thing if you print me. But I do do feel that I do do will be displayed wrong, displayed wrong...  or not."));
	FString Caption(TEXT("I am a big big caption in a big big game, it's not a big big thing if you print me. But I do do feel that I do do will be displayed wrong, displayed wrong... or not."));
	EAppReturnType::Type Result = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *Display, *Caption);

	UE_LOG(LogTestPAL, Display, TEXT("MessageBoxExt result: %d."), static_cast<int32>(Result));
	
	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * ************  Thread singleton test *****************
 */

/**
 * Per-thread singleton
 */
struct FPerThreadTestSingleton : public TThreadSingleton<FPerThreadTestSingleton>
{
	FPerThreadTestSingleton()
	{
		UE_LOG(LogTestPAL, Log, TEXT("FPerThreadTestSingleton (this=%p) created for thread %d"),
			this,
			FPlatformTLS::GetCurrentThreadId());
	}

	void DoSomething()
	{
		UE_LOG(LogTestPAL, Log, TEXT("Thread %d is about to quit"), FPlatformTLS::GetCurrentThreadId());
	}

	virtual ~FPerThreadTestSingleton()
	{
		UE_LOG(LogTestPAL, Log, TEXT("FPerThreadTestSingleton (%p) destroyed for thread %d"),
			this,
			FPlatformTLS::GetCurrentThreadId());
	}
};

//DECLARE_THREAD_SINGLETON( FPerThreadTestSingleton );

/**
 * Thread runnable
 */
struct FSingletonTestingThread : public FRunnable
{
	virtual uint32 Run()
	{
		FPerThreadTestSingleton& Dummy = FPerThreadTestSingleton::Get();

		FPlatformProcess::Sleep(3.0f);

		Dummy.DoSomething();
		return 0;
	}
};

/**
 * Thread singleton test
 */
int32 ThreadSingletonTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running thread singleton test."));

	const int kNumTestThreads = 10;

	FSingletonTestingThread * RunnableArray[kNumTestThreads] = { nullptr };
	FRunnableThread * ThreadArray[kNumTestThreads] = { nullptr };

	// start all threads
	for (int Idx = 0; Idx < kNumTestThreads; ++Idx)
	{
		RunnableArray[Idx] = new FSingletonTestingThread();
		ThreadArray[Idx] = FRunnableThread::Create(RunnableArray[Idx],
			*FString::Printf(TEXT("TestThread%d"), Idx));
	}

	GLog->FlushThreadedLogs();
	GLog->Flush();

	// join all threads
	for (int Idx = 0; Idx < kNumTestThreads; ++Idx)
	{
		ThreadArray[Idx]->WaitForCompletion();
		delete ThreadArray[Idx];
		ThreadArray[Idx] = nullptr;
		delete RunnableArray[Idx];
		RunnableArray[Idx] = nullptr;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Sysinfo test
 */
int32 SysInfoTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running system info test."));

	bool bIsRunningOnBattery = FPlatformMisc::IsRunningOnBattery();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::IsRunningOnBattery() = %s"), bIsRunningOnBattery ? TEXT("true") : TEXT("false"));

	GenericApplication * PlatformApplication = FPlatformMisc::CreateApplication();
	checkf(PlatformApplication, TEXT("Could not create platform application!"));
	bool bIsMouseAttached = PlatformApplication->IsMouseAttached();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::IsMouseAttached() = %s"), bIsMouseAttached ? TEXT("true") : TEXT("false"));

	FString OSInstanceGuid = FPlatformMisc::GetOperatingSystemId();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::GetOperatingSystemId() = '%s'"), *OSInstanceGuid);

	FString MacAddress = FPlatformMisc::GetMacAddressString();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::GetMacAddress() = '%s'"), *MacAddress);

	FString UserDir = FPlatformProcess::UserDir();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::UserDir() = '%s'"), *UserDir);

	FString ApplicationSettingsDir = FPlatformProcess::ApplicationSettingsDir();
	UE_LOG(LogTestPAL, Display, TEXT("  FPlatformMisc::ApplicationSettingsDir() = '%s'"), *ApplicationSettingsDir);

	FPlatformMemory::DumpStats(*GLog);

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Crash test
 */
int32 CrashTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running crash test (this should not exit)."));

	// try ensures first (each ensure fires just once)
	{
		for (int IdxEnsure = 0; IdxEnsure < 5; ++IdxEnsure)
		{
			FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled FIRST ensure() #%d times"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
			ensure(false);
		}
	}
	{
		for (int IdxEnsure = 0; IdxEnsure < 5; ++IdxEnsure)
		{
			FScopeLogTime EnsureLogTime(*FString::Printf(TEXT("Handled SECOND ensure() #%d times"), IdxEnsure), nullptr, FScopeLogTime::ScopeLog_Seconds);
			ensure(false);
		}
	}

	if (FParse::Param(CommandLine, TEXT("logfatal")))
	{
		UE_LOG(LogTestPAL, Fatal, TEXT("  LogFatal!"));
	}
	else if (FParse::Param(CommandLine, TEXT("check")))
	{
		checkf(false, TEXT("  checkf!"));
	}
	else
	{
		*(int *)0x10 = 0x11;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * String Precision test
 */
int32 StringPrecisionTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Running string precision test."));

	const FString TestString(TEXT("TestString"));
	int32 Indent = 15;
	UE_LOG(LogTestPAL, Display, TEXT("%*s"), Indent, *TestString);
	UE_LOG(LogTestPAL, Display, TEXT("Begining of the line %*s"), Indent, *TestString);
	UE_LOG(LogTestPAL, Display, TEXT("%*s end of the line"), Indent, *TestString);

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}

/**
 * Test Push/PopDll
 */
int32 DynamicLibraryTest(const TCHAR* CommandLine)
{
	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();

	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogTestPAL, Display, TEXT("Attempting to load Steam library"));

	FString RootSteamPath;
	FString LibraryName;

	if (PLATFORM_LINUX)
	{
		RootSteamPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/Steamworks/Steamv132/Linux/"));
		LibraryName = TEXT("libsteam_api.so");
	}
	else
	{
		UE_LOG(LogTestPAL, Fatal, TEXT("This test is not implemented for this platform."))
	}

	FPlatformProcess::PushDllDirectory(*RootSteamPath);
	void* SteamDLLHandle = FPlatformProcess::GetDllHandle(*LibraryName);
	FPlatformProcess::PopDllDirectory(*RootSteamPath);

	if (SteamDLLHandle == nullptr)
	{
		// try bundled one
		UE_LOG(LogTestPAL, Error, TEXT("Could not load via Push/PopDll, loading directly."));
		SteamDLLHandle = FPlatformProcess::GetDllHandle(*(RootSteamPath + LibraryName));

		if (SteamDLLHandle == nullptr)
		{
			UE_LOG(LogTestPAL, Fatal, TEXT("Could not load Steam library!"))
		}
	}

	if (SteamDLLHandle)
	{
		UE_LOG(LogTestPAL, Log, TEXT("Loaded Steam library at %p"), SteamDLLHandle);
		FPlatformProcess::FreeDllHandle(SteamDLLHandle);
		SteamDLLHandle = nullptr;
	}

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 0;
}



/**
 * Selects and runs one of test cases.
 *
 * @param ArgC Number of commandline arguments.
 * @param ArgV Commandline arguments.
 * @return Test case's return code.
 */
int32 MultiplexedMain(int32 ArgC, char* ArgV[])
{
	for (int32 IdxArg = 0; IdxArg < ArgC; ++IdxArg)
	{
		if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_PROC_TEST_CHILD))
		{
			return ProcRunAsChild(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_PROC_TEST))
		{
			return ProcRunAsParent(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_CASE_SENSITIVITY_TEST))
		{
			return CaseTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_MESSAGEBOX_TEST))
		{
			return MessageBoxTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_DIRECTORY_WATCHER_TEST))
		{
			return DirectoryWatcherTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_THREAD_SINGLETON_TEST))
		{
			return ThreadSingletonTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_SYSINFO_TEST))
		{
			return SysInfoTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_CRASH_TEST))
		{
			return CrashTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_STRINGPRECISION_TEST))
		{
			return StringPrecisionTest(*TestPAL::CommandLine);
		}
		else if (!FCStringAnsi::Strcmp(ArgV[IdxArg], ARG_DSO_TEST))
		{
			return DynamicLibraryTest(*TestPAL::CommandLine);
		}
	}

	FPlatformMisc::SetCrashHandler(NULL);
	FPlatformMisc::SetGracefulTerminationHandler();
	
	GEngineLoop.PreInit(*TestPAL::CommandLine);
	UE_LOG(LogTestPAL, Warning, TEXT("Unable to find any known test name, no test started."));

	UE_LOG(LogTestPAL, Warning, TEXT(""));
	UE_LOG(LogTestPAL, Warning, TEXT("Available test cases:"));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test process handling API"), ANSI_TO_TCHAR( ARG_PROC_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test case-insensitive file operations"), ANSI_TO_TCHAR( ARG_CASE_SENSITIVITY_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test message box bug (too long strings)"), ANSI_TO_TCHAR( ARG_MESSAGEBOX_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test directory watcher"), ANSI_TO_TCHAR( ARG_DIRECTORY_WATCHER_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test per-thread singletons"), ANSI_TO_TCHAR( ARG_THREAD_SINGLETON_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test (some) system information"), ANSI_TO_TCHAR( ARG_SYSINFO_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test crash handling (pass '-logfatal' for testing Fatal logs)"), ANSI_TO_TCHAR( ARG_CRASH_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test passing %%*s in a format string"), ANSI_TO_TCHAR( ARG_STRINGPRECISION_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT("  %s: test APIs for dealing with dynamic libraries"), ANSI_TO_TCHAR( ARG_DSO_TEST ));
	UE_LOG(LogTestPAL, Warning, TEXT(""));
	UE_LOG(LogTestPAL, Warning, TEXT("Pass one of those to run an appropriate test."));

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();
	return 1;
}

int32 main(int32 ArgC, char* ArgV[])
{
	TestPAL::CommandLine = TEXT("");

	for (int32 Option = 1; Option < ArgC; Option++)
	{
		TestPAL::CommandLine += TEXT(" ");
		FString Argument(ANSI_TO_TCHAR(ArgV[Option]));
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split( TEXT("="), &ArgName, &ArgValue );
				Argument = FString::Printf( TEXT("%s=\"%s\""), *ArgName, *ArgValue );
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		TestPAL::CommandLine += Argument;
	}

	return MultiplexedMain(ArgC, ArgV);
}
