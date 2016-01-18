// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Linux/LinuxPlatformCrashContext.h"
#include "Misc/App.h"
#include "EngineVersion.h"

#include <sys/utsname.h>	// for uname()
#include <signal.h>


FString DescribeSignal(int32 Signal, siginfo_t* Info)
{
	FString ErrorString;

#define HANDLE_CASE(a,b) case a: ErrorString += TEXT(#a ": " b); break;

	switch (Signal)
	{
	case 0:
		// No signal - used for initialization stacktrace on non-fatal errors (ex: ensure)
		break;
	case SIGSEGV:
		ErrorString += TEXT("SIGSEGV: invalid attempt to access memory at address ");
		ErrorString += FString::Printf(TEXT("0x%08x"), (uint32*)Info->si_addr);
		break;
	case SIGBUS:
		ErrorString += TEXT("SIGBUS: invalid attempt to access memory at address ");
		ErrorString += FString::Printf(TEXT("0x%08x"), (uint32*)Info->si_addr);
		break;

		HANDLE_CASE(SIGINT, "program interrupted")
		HANDLE_CASE(SIGQUIT, "user-requested crash")
		HANDLE_CASE(SIGILL, "illegal instruction")
		HANDLE_CASE(SIGTRAP, "trace trap")
		HANDLE_CASE(SIGABRT, "abort() called")
		HANDLE_CASE(SIGFPE, "floating-point exception")
		HANDLE_CASE(SIGKILL, "program killed")
		HANDLE_CASE(SIGSYS, "non-existent system call invoked")
		HANDLE_CASE(SIGPIPE, "write on a pipe with no reader")
		HANDLE_CASE(SIGTERM, "software termination signal")
		HANDLE_CASE(SIGSTOP, "stop")

	default:
		ErrorString += FString::Printf(TEXT("Signal %d (unknown)"), Signal);
	}

	return ErrorString;
#undef HANDLE_CASE
}

FLinuxCrashContext::~FLinuxCrashContext()
{
	if (BacktraceSymbols)
	{
		// glibc uses malloc() to allocate this, and we only need to free one pointer, see http://www.gnu.org/software/libc/manual/html_node/Backtraces.html
		free(BacktraceSymbols);
		BacktraceSymbols = NULL;
	}
}

void FLinuxCrashContext::InitFromSignal(int32 InSignal, siginfo_t* InInfo, void* InContext)
{
	Signal = InSignal;
	Info = InInfo;
	Context = reinterpret_cast< ucontext_t* >( InContext );

	FCString::Strcat(SignalDescription, ARRAY_COUNT( SignalDescription ) - 1, *DescribeSignal(Signal, Info));
}

/**
 * Handles graceful termination. Gives time to exit gracefully, but second signal will quit immediately.
 */
void GracefulTerminationHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	printf("CtrlCHandler: Signal=%d\n", Signal);

	// make sure as much data is written to disk as possible
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
	}

	if( !GIsRequestingExit )
	{
		GIsRequestingExit = 1;
	}
	else
	{
		FPlatformMisc::RequestExit(true);
	}
}

void CreateExceptionInfoString(int32 Signal, siginfo_t* Info)
{
	FString ErrorString = TEXT("Unhandled Exception: ");
	ErrorString += DescribeSignal(Signal, Info);
	FCString::Strncpy(GErrorExceptionDescription, *ErrorString, FMath::Min(ErrorString.Len() + 1, (int32)ARRAY_COUNT(GErrorExceptionDescription)));
}

namespace
{
	/** 
	 * Write a line of UTF-8 to a file
	 */
	void WriteLine(FArchive* ReportFile, const ANSICHAR* Line = NULL)
	{
		if( Line != NULL )
		{
			int64 StringBytes = FCStringAnsi::Strlen(Line);
			ReportFile->Serialize(( void* )Line, StringBytes);
		}

		// use Windows line terminator
		static ANSICHAR WindowsTerminator[] = "\r\n";
		ReportFile->Serialize(WindowsTerminator, 2);
	}

	/**
	 * Serializes UTF string to UTF-16
	 */
	void WriteUTF16String(FArchive* ReportFile, const TCHAR * UTFString4BytesChar, uint32 NumChars)
	{
		check(UTFString4BytesChar != NULL || NumChars == 0);
		static_assert(sizeof(TCHAR) == 4, "Platform TCHAR is not 4 bytes. Revisit this function.");

		for (uint32 Idx = 0; Idx < NumChars; ++Idx)
		{
			ReportFile->Serialize(const_cast< TCHAR* >( &UTFString4BytesChar[Idx] ), 2);
		}
	}

	/** 
	 * Writes UTF-16 line to a file
	 */
	void WriteLine(FArchive* ReportFile, const TCHAR* Line)
	{
		if( Line != NULL )
		{
			int64 NumChars = FCString::Strlen(Line);
			WriteUTF16String(ReportFile, Line, NumChars);
		}

		// use Windows line terminator
		static TCHAR WindowsTerminator[] = TEXT("\r\n");
		WriteUTF16String(ReportFile, WindowsTerminator, 2);
	}
}

/** 
 * Write all the data mined from the minidump to a text file
 */
void FLinuxCrashContext::GenerateReport(const FString & DiagnosticsPath) const
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter(*DiagnosticsPath);
	if (ReportFile != NULL)
	{
		FString Line;

		WriteLine(ReportFile, "Generating report for minidump");
		WriteLine(ReportFile);

		Line = FString::Printf(TEXT("Application version %d.%d.%d.0" ), FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor(), FEngineVersion::Current().GetPatch());
		WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));

		Line = FString::Printf(TEXT(" ... built from changelist %d"), FEngineVersion::Current().GetChangelist());
		WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));
		WriteLine(ReportFile);

		utsname UnixName;
		if (uname(&UnixName) == 0)
		{
			Line = FString::Printf(TEXT( "OS version %s %s (network name: %s)" ), ANSI_TO_TCHAR(UnixName.sysname), ANSI_TO_TCHAR(UnixName.release), ANSI_TO_TCHAR(UnixName.nodename));
			WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));	
			Line = FString::Printf( TEXT( "Running %d %s processors (%d logical cores)" ), FPlatformMisc::NumberOfCores(), ANSI_TO_TCHAR(UnixName.machine), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
			WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));
		}
		else
		{
			Line = FString::Printf(TEXT("OS version could not be determined (%d, %s)"), errno, ANSI_TO_TCHAR(strerror(errno)));
			WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));	
			Line = FString::Printf( TEXT( "Running %d unknown processors" ), FPlatformMisc::NumberOfCores());
			WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));
		}
		Line = FString::Printf(TEXT("Exception was \"%s\""), SignalDescription);
		WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));
		WriteLine(ReportFile);

		WriteLine(ReportFile, "<SOURCE START>");
		WriteLine(ReportFile, "<SOURCE END>");
		WriteLine(ReportFile);

		WriteLine(ReportFile, "<CALLSTACK START>");
		WriteLine(ReportFile, MinidumpCallstackInfo);
		WriteLine(ReportFile, "<CALLSTACK END>");
		WriteLine(ReportFile);

		WriteLine(ReportFile, "0 loaded modules");

		WriteLine(ReportFile);

		Line = FString::Printf(TEXT("Report end!"));
		WriteLine(ReportFile, TCHAR_TO_UTF8(*Line));

		ReportFile->Close();
		delete ReportFile;
	}
}

/** 
 * Mimics Windows WER format
 */
void GenerateWindowsErrorReport(const FString & WERPath)
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter(*WERPath);
	if (ReportFile != NULL)
	{
		// write BOM
		static uint16 ByteOrderMarker = 0xFEFF;
		ReportFile->Serialize(&ByteOrderMarker, sizeof(ByteOrderMarker));

		WriteLine(ReportFile, TEXT("<?xml version=\"1.0\" encoding=\"UTF-16\"?>"));
		WriteLine(ReportFile, TEXT("<WERReportMetadata>"));
		
		WriteLine(ReportFile, TEXT("\t<OSVersionInformation>"));
		WriteLine(ReportFile, TEXT("\t\t<WindowsNTVersion>0.0</WindowsNTVersion>"));
		WriteLine(ReportFile, TEXT("\t\t<Build>No Build</Build>"));
		WriteLine(ReportFile, TEXT("\t\t<Product>Linux</Product>"));
		WriteLine(ReportFile, TEXT("\t\t<Edition>No Edition</Edition>"));
		WriteLine(ReportFile, TEXT("\t\t<BuildString>No BuildString</BuildString>"));
		WriteLine(ReportFile, TEXT("\t\t<Revision>0</Revision>"));
		WriteLine(ReportFile, TEXT("\t\t<Flavor>No Flavor</Flavor>"));
		WriteLine(ReportFile, TEXT("\t\t<Architecture>Unknown Architecture</Architecture>"));
		WriteLine(ReportFile, TEXT("\t\t<LCID>0</LCID>"));
		WriteLine(ReportFile, TEXT("\t</OSVersionInformation>"));
		
		WriteLine(ReportFile, TEXT("\t<ParentProcessInformation>"));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<ParentProcessId>%d</ParentProcessId>"), getppid()));
		WriteLine(ReportFile, TEXT("\t\t<ParentProcessPath>C:\\Windows\\explorer.exe</ParentProcessPath>"));			// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<ParentProcessCmdLine>C:\\Windows\\Explorer.EXE</ParentProcessCmdLine>"));	// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t</ParentProcessInformation>"));

		WriteLine(ReportFile, TEXT("\t<ProblemSignatures>"));
		WriteLine(ReportFile, TEXT("\t\t<EventType>APPCRASH</EventType>"));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<Parameter0>UE4-%s</Parameter0>"), FApp::GetGameName()));
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<Parameter1>%d.%d.%d</Parameter1>"), FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor(), FEngineVersion::Current().GetPatch()));
		WriteLine(ReportFile, TEXT("\t\t<Parameter2>0</Parameter2>"));													// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter3>Unknown Fault Module</Parameter3>"));										// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter4>0.0.0.0</Parameter4>"));													// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter5>00000000</Parameter5>"));													// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter6>00000000</Parameter6>"));													// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<Parameter7>0000000000000000</Parameter7>"));											// FIXME: supply valid?
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<Parameter8>!%s!</Parameter8>"), FCommandLine::Get()));				// FIXME: supply valid? Only partially valid
		WriteLine(ReportFile, *FString::Printf(TEXT("\t\t<Parameter9>%s!%s!%s!%d</Parameter9>"), *FApp::GetBranchName(), FPlatformProcess::BaseDir(), FPlatformMisc::GetEngineMode(), FEngineVersion::Current().GetChangelist()));
		WriteLine(ReportFile, TEXT("\t</ProblemSignatures>"));

		WriteLine(ReportFile, TEXT("\t<DynamicSignatures>"));
		WriteLine(ReportFile, TEXT("\t\t<Parameter1>6.1.7601.2.1.0.256.48</Parameter1>"));
		WriteLine(ReportFile, TEXT("\t\t<Parameter2>1033</Parameter2>"));
		WriteLine(ReportFile, TEXT("\t</DynamicSignatures>"));

		WriteLine(ReportFile, TEXT("\t<SystemInformation>"));
		WriteLine(ReportFile, TEXT("\t\t<MID>11111111-2222-3333-4444-555555555555</MID>"));							// FIXME: supply valid?
		
		WriteLine(ReportFile, TEXT("\t\t<SystemManufacturer>Unknown.</SystemManufacturer>"));						// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<SystemProductName>Linux machine</SystemProductName>"));					// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t\t<BIOSVersion>A02</BIOSVersion>"));											// FIXME: supply valid?
		WriteLine(ReportFile, TEXT("\t</SystemInformation>"));

		WriteLine(ReportFile, TEXT("</WERReportMetadata>"));

		ReportFile->Close();
		delete ReportFile;
	}
}

/** 
 * Creates (fake so far) minidump
 */
void GenerateMinidump(const FString & Path)
{
	FArchive* ReportFile = IFileManager::Get().CreateFileWriter(*Path);
	if (ReportFile != NULL)
	{
		// write BOM
		static uint32 Garbage = 0xDEADBEEF;
		ReportFile->Serialize(&Garbage, sizeof(Garbage));

		ReportFile->Close();
		delete ReportFile;
	}
}


int32 DLLEXPORT ReportCrash(const FLinuxCrashContext & Context)
{
	static bool GAlreadyCreatedMinidump = false;
	// Only create a minidump the first time this function is called.
	// (Can be called the first time from the RenderThread, then a second time from the MainThread.)
	if ( GAlreadyCreatedMinidump == false )
	{
		GAlreadyCreatedMinidump = true;

		const SIZE_T StackTraceSize = 65535;
		ANSICHAR* StackTrace = (ANSICHAR*) FMemory::Malloc( StackTraceSize );
		StackTrace[0] = 0;
		// Walk the stack and dump it to the allocated memory (ignore first 2 callstack lines as those are in stack walking code)
		FPlatformStackWalk::StackWalkAndDump( StackTrace, StackTraceSize, 2, const_cast< FLinuxCrashContext* >( &Context ) );

		FCString::Strncat( GErrorHist, ANSI_TO_TCHAR(StackTrace), ARRAY_COUNT(GErrorHist) - 1 );
		CreateExceptionInfoString(Context.Signal, Context.Info);

		FMemory::Free( StackTrace );
	}

	return 0;
}

/**
 * Generates information for crash reporter
 */
void DLLEXPORT GenerateCrashInfoAndLaunchReporter(const FLinuxCrashContext & Context)
{
	// do not report crashes for tools (particularly for crash reporter itself)
#if !IS_PROGRAM

	// create a crash-specific directory
	FString CrashInfoFolder = FString::Printf(TEXT("crashinfo-%s-pid-%d-%s"), FApp::GetGameName(), getpid(), *FGuid::NewGuid().ToString());
	FString CrashInfoAbsolute = FPaths::ConvertRelativePathToFull(CrashInfoFolder);
	if (IFileManager::Get().MakeDirectory(*CrashInfoFolder))
	{
		// generate "minidump"
		Context.GenerateReport(FPaths::Combine(*CrashInfoFolder, TEXT("diagnostics.txt")));

		// generate "WER"
		GenerateWindowsErrorReport(FPaths::Combine(*CrashInfoFolder, TEXT("wermeta.xml")));

		// generate "minidump" (just >1 byte)
		GenerateMinidump(FPaths::Combine(*CrashInfoFolder, TEXT("minidump.dmp")));

		// Introduces a new runtime crash context. Will replace all Windows related crash reporting.
		//FCStringAnsi::Strncpy(FilePath, CrashInfoFolder, PATH_MAX);
		//FCStringAnsi::Strcat(FilePath, PATH_MAX, "/" );
		//FCStringAnsi::Strcat(FilePath, PATH_MAX, FGenericCrashContext::CrashContextRuntimeXMLNameA );
		//SerializeAsXML( FilePath ); @todo uncomment after verification

		// copy log
		FString LogSrcAbsolute = FPlatformOutputDevices::GetAbsoluteLogFilename();
		FString LogDstAbsolute = FPaths::Combine(*CrashInfoAbsolute, *FPaths::GetCleanFilename(LogSrcAbsolute));
		FPaths::NormalizeDirectoryName(LogDstAbsolute);
		static_cast<void>(IFileManager::Get().Copy(*LogDstAbsolute, *LogSrcAbsolute));	// best effort, so don't care about result: couldn't copy -> tough, no log

		// try launching the tool and wait for its exit, if at all
		const TCHAR * RelativePathToCrashReporter = TEXT("../../../Engine/Binaries/Linux/CrashReportClient");	// FIXME: painfully hard-coded
		if (!FPaths::FileExists(RelativePathToCrashReporter))
		{
			RelativePathToCrashReporter = TEXT("../../../engine/binaries/linux/crashreportclient");	// FIXME: even more painfully hard-coded
		}

		FString CrashReportClientArguments;

		// Suppress the user input dialog if we're running in unattended mode
		bool bNoDialog = FApp::IsUnattended() || IsRunningDedicatedServer();
		if (bNoDialog)
		{
			CrashReportClientArguments += TEXT(" -Unattended ");
		}

		CrashReportClientArguments += CrashInfoAbsolute + TEXT("/");

		// show on the console
		printf("Starting %s\n", StringCast<ANSICHAR>(RelativePathToCrashReporter).Get());
		FProcHandle RunningProc = FPlatformProcess::CreateProc(RelativePathToCrashReporter, *CrashReportClientArguments, true, false, false, NULL, 0, NULL, NULL);
		if (FPlatformProcess::IsProcRunning(RunningProc))
		{
			// do not wait indefinitely
			double kTimeOut = 3 * 60.0;
			double StartSeconds = FPlatformTime::Seconds();
			for(;;)
			{
				if (!FPlatformProcess::IsProcRunning(RunningProc))
				{
					break;
				}

				if (FPlatformTime::Seconds() - StartSeconds > kTimeOut)
				{
					break;
				}

				FPlatformProcess::Sleep(1.0f);
			};
		}
	}

#endif

	FPlatformMisc::RequestExit(true);
}

/**
 * Good enough default crash reporter.
 */
void DefaultCrashHandler(const FLinuxCrashContext & Context)
{
	// Switch to malloc crash.
	//FMallocCrash::Get().SetAsGMalloc(); 

	printf("DefaultCrashHandler: Signal=%d\n", Context.Signal);

	ReportCrash(Context);
	if (GLog)
	{
		GLog->Flush();
	}
	if (GWarn)
	{
		GWarn->Flush();
	}
	if (GError)
	{
		GError->Flush();
		GError->HandleError();
	}

	return GenerateCrashInfoAndLaunchReporter(Context);
}

/** Global pointer to crash handler */
void (* GCrashHandlerPointer)(const FGenericCrashContext & Context) = NULL;

/** True system-specific crash handler that gets called first */
void PlatformCrashHandler(int32 Signal, siginfo_t* Info, void* Context)
{
	fprintf(stderr, "Signal %d caught.\n", Signal);

	FLinuxCrashContext CrashContext;
	CrashContext.InitFromSignal(Signal, Info, Context);

	if (GCrashHandlerPointer)
	{
		GCrashHandlerPointer(CrashContext);
	}
	else
	{
		// call default one
		DefaultCrashHandler(CrashContext);
	}
}

void FLinuxPlatformMisc::SetGracefulTerminationHandler()
{
	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	Action.sa_sigaction = GracefulTerminationHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	sigaction(SIGINT, &Action, NULL);
	sigaction(SIGTERM, &Action, NULL);
	sigaction(SIGHUP, &Action, NULL);	//  this should actually cause the server to just re-read configs (restart?)
}

void FLinuxPlatformMisc::SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext & Context))
{
	GCrashHandlerPointer = CrashHandler;

	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	Action.sa_sigaction = PlatformCrashHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	sigaction(SIGQUIT, &Action, NULL);	// SIGQUIT is a user-initiated "crash".
	sigaction(SIGILL, &Action, NULL);
	sigaction(SIGFPE, &Action, NULL);
	sigaction(SIGBUS, &Action, NULL);
	sigaction(SIGSEGV, &Action, NULL);
	sigaction(SIGSYS, &Action, NULL);
}