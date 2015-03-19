// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "LinuxPlatformRunnableThread.h"
#include "Public/Modules/ModuleVersion.h"
#include <spawn.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/ioctl.h> // ioctl
#include <asm/ioctls.h> // FIONREAD

void* FLinuxPlatformProcess::GetDllHandle( const TCHAR* Filename )
{
	check( Filename );
	FString AbsolutePath = FPaths::ConvertRelativePathToFull(Filename);
	void *Handle = dlopen( TCHAR_TO_ANSI(*AbsolutePath), RTLD_LAZY | RTLD_LOCAL );
	if (!Handle)
	{
		UE_LOG(LogLinux, Warning, TEXT("dlopen failed: %s"), ANSI_TO_TCHAR(dlerror()) );
	}
	return Handle;
}

void FLinuxPlatformProcess::FreeDllHandle( void* DllHandle )
{
	check( DllHandle );
	dlclose( DllHandle );
}

void* FLinuxPlatformProcess::GetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	check(DllHandle);
	check(ProcName);
	return dlsym( DllHandle, TCHAR_TO_ANSI(ProcName) );
}

int32 FLinuxPlatformProcess::GetDllApiVersion( const TCHAR* Filename )
{
	check(Filename);
	return MODULE_API_VERSION;
}

const TCHAR* FLinuxPlatformProcess::GetModulePrefix()
{
	return TEXT("lib");
}

const TCHAR* FLinuxPlatformProcess::GetModuleExtension()
{
	return TEXT("so");
}

const TCHAR* FLinuxPlatformProcess::GetBinariesSubdirectory()
{
	return TEXT("Linux");
}

namespace PlatformProcessLimits
{
	enum
	{
		MaxComputerName	= 128,
		MaxBaseDirLength= MAX_PATH + 1,
		MaxArgvParameters = 256
	};
};

const TCHAR* FLinuxPlatformProcess::ComputerName()
{
	static bool bHaveResult = false;
	static TCHAR CachedResult[ PlatformProcessLimits::MaxComputerName ];
	if (!bHaveResult)
	{
		struct utsname name;
		const char * SysName = name.nodename;
		if(uname(&name))
		{
			SysName = "Linux Computer";
		}

		FCString::Strcpy(CachedResult, ARRAY_COUNT(CachedResult) - 1, ANSI_TO_TCHAR(SysName));
		CachedResult[ARRAY_COUNT(CachedResult) - 1] = 0;
		bHaveResult = true;
	}
	return CachedResult;
}

void FLinuxPlatformProcess::CleanFileCache()
{
	bool bShouldCleanShaderWorkingDirectory = true;
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
	// Only clean the shader working directory if we are the first instance, to avoid deleting files in use by other instances
	// @todo - check if any other instances are running right now
	bShouldCleanShaderWorkingDirectory = GIsFirstInstance;
#endif

	if (bShouldCleanShaderWorkingDirectory && !FParse::Param( FCommandLine::Get(), TEXT("Multiprocess")))
	{
		// get shader path, and convert it to the userdirectory
		FString ShaderDir = FString(FPlatformProcess::BaseDir()) / FPlatformProcess::ShaderDir();
		FString UserShaderDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ShaderDir);
		FPaths::CollapseRelativeDirectories(ShaderDir);

		// make sure we don't delete from the source directory
		if (ShaderDir != UserShaderDir)
		{
			IFileManager::Get().DeleteDirectory(*UserShaderDir, false, true);
		}

		FPlatformProcess::CleanShaderWorkingDir();
	}
}

const TCHAR* FLinuxPlatformProcess::BaseDir()
{
	static bool bHaveResult = false;
	static TCHAR CachedResult[ PlatformProcessLimits::MaxBaseDirLength ];

	if (!bHaveResult)
	{
		char SelfPath[ PlatformProcessLimits::MaxBaseDirLength ] = {0};
		if (readlink( "/proc/self/exe", SelfPath, ARRAY_COUNT(SelfPath) - 1) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Fatal, TEXT("readlink() failed with errno = %d (%s)"), ErrNo,
				StringCast< TCHAR >(strerror(ErrNo)).Get());
			// unreachable
			return CachedResult;
		}
		SelfPath[ARRAY_COUNT(SelfPath) - 1] = 0;

		FCString::Strcpy(CachedResult, ARRAY_COUNT(CachedResult) - 1, ANSI_TO_TCHAR(dirname(SelfPath)));
		CachedResult[ARRAY_COUNT(CachedResult) - 1] = 0;
		FCString::Strcat(CachedResult, ARRAY_COUNT(CachedResult) - 1, TEXT("/"));
		bHaveResult = true;
	}
	return CachedResult;
}

const TCHAR* FLinuxPlatformProcess::UserDir()
{
	// The UserDir is where user visible files (such as game projects) live.
	// On Linux (just like on Mac) this corresponds to $HOME/Documents.
	// To accomodate localization requirement we use xdg-user-dir command,
	// and fall back to $HOME/Documents if setting not found.
	static TCHAR Result[MAX_PATH] = TEXT("");

	if (!Result[0])
	{
		char DocPath[MAX_PATH];
		
		FILE* FilePtr = popen("xdg-user-dir DOCUMENTS", "r");
		if(fgets(DocPath, MAX_PATH, FilePtr) == NULL)
		{
			char* Home = secure_getenv("HOME");
			if (!Home)
			{
				UE_LOG(LogHAL, Warning, TEXT("Unable to read the $HOME environment variable"));
			}
			else
			{
				FCString::Strcpy(Result, ANSI_TO_TCHAR(Home));
				FCString::Strcat(Result, TEXT("/Documents/"));
			}
		}
		else
		{
				size_t DocLen = strlen(DocPath) - 1;
				DocPath[DocLen] = '\0';
				FCString::Strcpy(Result, ANSI_TO_TCHAR(DocPath));
				FCString::Strcat(Result, TEXT("/"));
		}
		pclose(FilePtr);
	}
	return Result;
}

const TCHAR* FLinuxPlatformProcess::UserSettingsDir()
{
	// Like on Mac we use the same folder for UserSettingsDir and ApplicationSettingsDir
	// $HOME/.config/Epic/
	return ApplicationSettingsDir();
}

const TCHAR* FLinuxPlatformProcess::ApplicationSettingsDir()
{
	// The ApplicationSettingsDir is where the engine stores settings and configuration
	// data.  On linux this corresponds to $HOME/.config/Epic
	static TCHAR Result[MAX_PATH] = TEXT("");
	if (!Result[0])
	{
		char* Home = secure_getenv("HOME");
		if (!Home)
		{
			UE_LOG(LogHAL, Warning, TEXT("Unable to read the $HOME environment variable"));
		}
		else
		{
			FCString::Strcpy(Result, ANSI_TO_TCHAR(Home));
			FCString::Strcat(Result, TEXT("/.config/Epic/"));
		}
	}
	return Result;
}

bool FLinuxPlatformProcess::SetProcessLimits(EProcessResource::Type Resource, uint64 Limit)
{
	rlimit NativeLimit;
	NativeLimit.rlim_cur = Limit;
	NativeLimit.rlim_max = Limit;

	int NativeResource = RLIMIT_AS;

	switch(Resource)
	{
		case EProcessResource::VirtualMemory:
			NativeResource = RLIMIT_AS;
			break;

		default:
			UE_LOG(LogHAL, Warning, TEXT("Unknown resource type %d"), Resource);
			return false;
	}

	if (setrlimit(NativeResource, &NativeLimit) != 0)
	{
		UE_LOG(LogHAL, Warning, TEXT("setrlimit() failed with error %d (%s)\n"), errno, ANSI_TO_TCHAR(strerror(errno)));
		return false;
	}

	return true;
}

const TCHAR* FLinuxPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static bool bHaveResult = false;
	static TCHAR CachedResult[ PlatformProcessLimits::MaxBaseDirLength ];
	if (!bHaveResult)
	{
		char SelfPath[ PlatformProcessLimits::MaxBaseDirLength ] = {0};
		if (readlink( "/proc/self/exe", SelfPath, ARRAY_COUNT(SelfPath) - 1) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Fatal, TEXT("readlink() failed with errno = %d (%s)"), ErrNo,
				StringCast< TCHAR >(strerror(ErrNo)).Get());
			return CachedResult;
		}
		SelfPath[ARRAY_COUNT(SelfPath) - 1] = 0;

		FCString::Strcpy(CachedResult, ARRAY_COUNT(CachedResult) - 1, ANSI_TO_TCHAR(basename(SelfPath)));
		CachedResult[ARRAY_COUNT(CachedResult) - 1] = 0;
		bHaveResult = true;
	}
	return CachedResult;
}


FString FLinuxPlatformProcess::GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration)
{
	FString PlatformName = GetBinariesSubdirectory();
	FString ExecutablePath = FString::Printf(TEXT("../%s/%s"), *PlatformName, *AppName);
	
	if (BuildConfiguration != EBuildConfigurations::Development && BuildConfiguration != EBuildConfigurations::DebugGame)
	{
		ExecutablePath += FString::Printf(TEXT("-%s-%s"), *PlatformName, EBuildConfigurations::ToString(BuildConfiguration));
	}
	return ExecutablePath;
}


FString FLinuxPlatformProcess::GetApplicationName( uint32 ProcessId )
{
	FString Output = TEXT("");

	const int32 ReadLinkSize = 1024;	
	char ReadLinkCmd[ReadLinkSize] = {0};
	FCStringAnsi::Sprintf(ReadLinkCmd, "/proc/%d/exe", ProcessId);
	
	char ProcessPath[ PlatformProcessLimits::MaxBaseDirLength ] = {0};
	int32 Ret = readlink(ReadLinkCmd, ProcessPath, ARRAY_COUNT(ProcessPath) - 1);
	if (Ret != -1)
	{
		Output = ANSI_TO_TCHAR(ProcessPath);
	}
	return Output;
}

FPipeHandle::~FPipeHandle()
{
	close(PipeDesc);
}

FString FPipeHandle::Read()
{
	const int kBufferSize = 4096;
	ANSICHAR Buffer[kBufferSize];
	FString Output;

	int BytesAvailable = 0;
	if (ioctl(PipeDesc, FIONREAD, &BytesAvailable) == 0)
	{
		if (BytesAvailable > 0)
		{
			int BytesRead = read(PipeDesc, Buffer, kBufferSize - 1);
			if (BytesRead > 0)
			{
				Buffer[BytesRead] = 0;
				Output += StringCast< TCHAR >(Buffer).Get();
			}
		}
	}
	else
	{
		UE_LOG(LogHAL, Fatal, TEXT("ioctl(..., FIONREAD, ...) failed with errno=%d (%s)"), errno, StringCast< TCHAR >(strerror(errno)).Get());
	}

	return Output;
}

bool FPipeHandle::ReadToArray(TArray<uint8> & Output)
{
	int BytesAvailable = 0;
	if (ioctl(PipeDesc, FIONREAD, &BytesAvailable) == 0)
	{
		if (BytesAvailable > 0)
		{
			Output.Init(BytesAvailable);
			int BytesRead = read(PipeDesc, Output.GetData(), BytesAvailable);
			if (BytesRead > 0)
			{
				if (BytesRead < BytesAvailable)
				{
					Output.SetNum(BytesRead);
				}

				return true;
			}
			else
			{
				Output.Empty();
			}
		}
	}

	return false;
}


void FLinuxPlatformProcess::ClosePipe( void* ReadPipe, void* WritePipe )
{
	if (ReadPipe)
	{
		FPipeHandle * PipeHandle = reinterpret_cast< FPipeHandle* >(ReadPipe);
		delete PipeHandle;
	}

	if (WritePipe)
	{
		FPipeHandle * PipeHandle = reinterpret_cast< FPipeHandle* >(WritePipe);
		delete PipeHandle;
	}
}

bool FLinuxPlatformProcess::CreatePipe( void*& ReadPipe, void*& WritePipe )
{
	int PipeFd[2];
	if (-1 == pipe(PipeFd))
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Warning, TEXT("pipe() failed with errno = %d (%s)"), ErrNo, 
			StringCast< TCHAR >(strerror(ErrNo)).Get());
		return false;
	}

	ReadPipe = new FPipeHandle(PipeFd[ 0 ]);
	WritePipe = new FPipeHandle(PipeFd[ 1 ]);

	return true;
}

FString FLinuxPlatformProcess::ReadPipe( void* ReadPipe )
{
	if (ReadPipe)
	{
		FPipeHandle * PipeHandle = reinterpret_cast< FPipeHandle* >(ReadPipe);
		return PipeHandle->Read();
	}

	return FString();
}

bool FLinuxPlatformProcess::ReadPipeToArray(void* ReadPipe, TArray<uint8> & Output)
{
	if (ReadPipe)
	{
		FPipeHandle * PipeHandle = reinterpret_cast<FPipeHandle*>(ReadPipe);
		return PipeHandle->ReadToArray(Output);
	}

	return false;
}

FRunnableThread* FLinuxPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadLinux();
}

void FLinuxPlatformProcess::LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error)
{
	// @todo This ignores params and error; mostly a stub
	pid_t pid = fork();
	UE_LOG(LogHAL, Verbose, TEXT("FLinuxPlatformProcess::LaunchURL: '%s'"), TCHAR_TO_ANSI(URL));
	if (pid == 0)
	{
		exit(execl("/usr/bin/xdg-open", "xdg-open", TCHAR_TO_ANSI(URL), (char *)0));
	}
}

FProcHandle FLinuxPlatformProcess::CreateProc(const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWrite)
{
	// @TODO bLaunchHidden bLaunchReallyHidden are not handled
	// We need an absolute path to executable
	FString ProcessPath = URL;
	if (*URL != '/')
	{
		ProcessPath = FPaths::ConvertRelativePathToFull(ProcessPath);
	}

	if (!FPaths::FileExists(ProcessPath))
	{
		return FProcHandle();
	}

	FString Commandline = ProcessPath;
	Commandline += TEXT(" ");
	Commandline += Parms;

	UE_LOG(LogHAL, Verbose, TEXT("FLinuxPlatformProcess::CreateProc: '%s'"), *Commandline);

	TArray<FString> ArgvArray;
	int Argc = Commandline.ParseIntoArray(&ArgvArray, TEXT(" "), true);
	char* Argv[PlatformProcessLimits::MaxArgvParameters + 1] = { NULL };	// last argument is NULL, hence +1
	struct CleanupArgvOnExit
	{
		int Argc;
		char** Argv;	// relying on it being long enough to hold Argc elements

		CleanupArgvOnExit( int InArgc, char *InArgv[] )
			:	Argc(InArgc)
			,	Argv(InArgv)
		{}

		~CleanupArgvOnExit()
		{
			for (int Idx = 0; Idx < Argc; ++Idx)
			{
				FMemory::Free(Argv[Idx]);
			}
		}
	} CleanupGuard(Argc, Argv);

	// make sure we do not lose arguments with spaces in them due to Commandline.ParseIntoArray breaking them apart above
	// @todo this code might need to be optimized somehow and integrated with main argument parser below it
	TArray<FString> NewArgvArray;
	if (Argc > 0)
	{
		if (Argc > PlatformProcessLimits::MaxArgvParameters)
		{
			UE_LOG(LogHAL, Warning, TEXT("FLinuxPlatformProcess::CreateProc: too many (%d) commandline arguments passed, will only pass %d"),
				Argc, PlatformProcessLimits::MaxArgvParameters);
			Argc = PlatformProcessLimits::MaxArgvParameters;
		}

		FString MultiPartArg;
		for (int32 Index = 0; Index < Argc; Index++)
		{
			if (MultiPartArg.IsEmpty())
			{
				if ((ArgvArray[Index].StartsWith(TEXT("\"")) && !ArgvArray[Index].EndsWith(TEXT("\""))) // check for a starting quote but no ending quote, excludes quoted single arguments
					|| (ArgvArray[Index].Contains(TEXT("=\"")) && !ArgvArray[Index].EndsWith(TEXT("\""))) // check for quote after =, but no ending quote, this gets arguments of the type -blah="string string string"
					|| ArgvArray[Index].EndsWith(TEXT("=\""))) // check for ending quote after =, this gets arguments of the type -blah=" string string string "
				{
					MultiPartArg = ArgvArray[Index];
				}
				else
				{
					if (ArgvArray[Index].Contains(TEXT("=\"")))
					{
						FString SingleArg = ArgvArray[Index];
						SingleArg = SingleArg.Replace(TEXT("=\""), TEXT("="));
						NewArgvArray.Add(SingleArg.TrimQuotes(NULL));
					}
					else
					{
						NewArgvArray.Add(ArgvArray[Index].TrimQuotes(NULL));
					}
				}
			}
			else
			{
				MultiPartArg += TEXT(" ");
				MultiPartArg += ArgvArray[Index];
				if (ArgvArray[Index].EndsWith(TEXT("\"")))
				{
					if (MultiPartArg.StartsWith(TEXT("\"")))
					{
						NewArgvArray.Add(MultiPartArg.TrimQuotes(NULL));
					}
					else
					{
						NewArgvArray.Add(MultiPartArg);
					}
					MultiPartArg.Empty();
				}
			}
		}
	}
	// update Argc with the new argument count
	Argc = NewArgvArray.Num();

	if (Argc > 0)	// almost always, unless there's no program name
	{
		if (Argc > PlatformProcessLimits::MaxArgvParameters)
		{
			UE_LOG(LogHAL, Warning, TEXT("FLinuxPlatformProcess::CreateProc: too many (%d) commandline arguments passed, will only pass %d"), 
				Argc, PlatformProcessLimits::MaxArgvParameters);
			Argc = PlatformProcessLimits::MaxArgvParameters;
		}

		for (int Idx = 0; Idx < Argc; ++Idx)
		{
			auto AnsiBuffer = StringCast<ANSICHAR>(*NewArgvArray[Idx]);
			const char* Ansi = AnsiBuffer.Get();
			size_t AnsiSize = FCStringAnsi::Strlen(Ansi) + 1;
			check(AnsiSize);

			Argv[Idx] = reinterpret_cast< char* >( FMemory::Malloc(AnsiSize) );
			check(Argv[Idx]);

			FCStringAnsi::Strncpy(Argv[Idx], Ansi, AnsiSize);
		}

		// last Argv should be NULL
		check(Argc <= PlatformProcessLimits::MaxArgvParameters + 1);
		Argv[Argc] = NULL;
	}

	extern char ** environ;	// provided by libc
	pid_t ChildPid = -1;

	posix_spawn_file_actions_t FileActions;

	posix_spawn_file_actions_init(&FileActions);
	if (PipeWrite)
	{
		const FPipeHandle* PipeWriteHandle = reinterpret_cast< const FPipeHandle* >(PipeWrite);
		posix_spawn_file_actions_adddup2(&FileActions, PipeWriteHandle->GetHandle(), STDOUT_FILENO);
	}

	int ErrNo = posix_spawn(&ChildPid, TCHAR_TO_ANSI(*ProcessPath), &FileActions, NULL, Argv, environ);
	posix_spawn_file_actions_destroy(&FileActions);
	if (ErrNo != 0)
	{
		UE_LOG(LogHAL, Fatal, TEXT("FLinuxPlatformProcess::CreateProc: posix_spawn() failed (%d, %s)"), ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
		return FProcHandle();	// produce knowingly invalid handle if for some reason Fatal log (above) returns
	}
	else
	{
		UE_LOG(LogHAL, Log, TEXT("FLinuxPlatformProcess::CreateProc: spawned child %d"), ChildPid);
	}

	if (OutProcessID)
	{
		*OutProcessID = ChildPid;
	}

	return FProcHandle( ChildPid );
}

bool FProcHandle::IsRunning()
{
	if (bIsRunning)
	{
		check(!bHasBeenWaitedFor);	// check for the sake of internal consistency

		// check if actually running
		int KillResult = kill(Get(), 0);	// no actual signal is sent
		check(KillResult != -1 || errno != EINVAL);

		bIsRunning = (KillResult == 0 || (KillResult == -1 && errno == EPERM));

		// additional check if it's a zombie
		if (bIsRunning)
		{
			for(;;)	// infinite loop in case we get EINTR and have to repeat
			{
				siginfo_t SignalInfo;
				SignalInfo.si_pid = 0;	// if remains 0, treat as child was not waitable (i.e. was running)
				if (waitid(P_PID, Get(), &SignalInfo, WEXITED | WNOHANG | WNOWAIT))
				{
					if (errno != EINTR)
					{
						UE_LOG(LogHAL, Fatal, TEXT("FLinuxPlatformProcess::WaitForProc: waitid for pid %d failed (errno=%d, %s)"), 
							static_cast< int32 >(Get()), errno, ANSI_TO_TCHAR(strerror(errno)));
						break;	// exit the loop if for some reason Fatal log (above) returns
					}
				}
				else
				{
					bIsRunning = ( SignalInfo.si_pid != Get() );
					break;
				}
			}
		}

		// If child is a zombie, wait() immediately to free up kernel resources. Higher level code
		// (e.g. shader compiling manager) can hold on to handle of no longer running process for longer,
		// which is a dubious, but valid behavior. We don't want to keep zombie around though.
		if (!bIsRunning)
		{
			UE_LOG(LogHAL, Log, TEXT("Child %d is no more running (zombie), Wait()ing immediately."), Get() );
			Wait();
		}
	}

	return bIsRunning;
}

bool FProcHandle::GetReturnCode(int32* ReturnCodePtr)
{
	check(!bIsRunning || !"You cannot get a return code of a running process");
	if (!bHasBeenWaitedFor)
	{
		Wait();
	}

	if (ReturnCode != -1)
	{
		if (ReturnCodePtr != NULL)
		{
			*ReturnCodePtr = ReturnCode;
		}
		return true;
	}

	return false;
}

void FProcHandle::Wait()
{
	if (bHasBeenWaitedFor)
	{
		UE_LOG(LogHAL, Log, TEXT("FLinuxPlatformProcess::WaitForProc: already waited on process %d"), static_cast< int32 >(Get()));
		return;	// we could try waitpid() another time, but why
	}

	for(;;)	// infinite loop in case we get EINTR and have to repeat
	{
		siginfo_t SignalInfo;
		if (waitid(P_PID, Get(), &SignalInfo, WEXITED))
		{
			if (errno != EINTR)
			{
				UE_LOG(LogHAL, Fatal, TEXT("FLinuxPlatformProcess::WaitForProc: waitid for pid %d failed (errno=%d, %s)"), 
					static_cast< int32 >(Get()), errno, ANSI_TO_TCHAR(strerror(errno)));
				break;	// exit the loop if for some reason Fatal log (above) returns
			}
		}
		else
		{
			check(SignalInfo.si_pid == Get());

			ReturnCode = (SignalInfo.si_code == CLD_EXITED) ? SignalInfo.si_status : -1;
			bHasBeenWaitedFor = true;
			bIsRunning = false;	// set in advance
			break;
		}

		UE_LOG(LogHAL, Log, TEXT("FLinuxPlatformProcess::WaitForProc: process %d waited on and returned %d"), static_cast<int32>(Get()), ReturnCode);
	}
}

bool FLinuxPlatformProcess::IsProcRunning( FProcHandle & ProcessHandle )
{
	return ProcessHandle.IsRunning();
}

void FLinuxPlatformProcess::WaitForProc( FProcHandle & ProcessHandle )
{
	ProcessHandle.Wait();
}

void FLinuxPlatformProcess::TerminateProc( FProcHandle & ProcessHandle, bool KillTree )
{
	if (KillTree)
	{
		// TODO: enumerate the children
		STUBBED("FLinuxPlatformProcess::TerminateProc() : Killing a subtree is not implemented yet");
	}

	UE_LOG(LogHAL, Log, TEXT("FLinuxPlatformProcess::TerminateProc: terminated process %d"), static_cast< int32 >(ProcessHandle.Get()));
	int KillResult = kill(ProcessHandle.Get(), SIGTERM);	// graceful
	check(KillResult != -1 || errno != EINVAL);
}

uint32 FLinuxPlatformProcess::GetCurrentProcessId()
{
	return getpid();
}

bool FLinuxPlatformProcess::GetProcReturnCode( FProcHandle& ProcHandle, int32* ReturnCode )
{
	if (IsProcRunning(ProcHandle))
	{
		return false;
	}

	return ProcHandle.GetReturnCode(ReturnCode);
}

bool FLinuxPlatformProcess::Daemonize()
{
	if (daemon(1, 1) == -1)
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Warning, TEXT("daemon(1, 1) failed with errno = %d (%s)"), ErrNo,
			StringCast< TCHAR >(strerror(ErrNo)).Get());
		return false;
	}

	return true;
}

bool FLinuxPlatformProcess::IsApplicationRunning( uint32 ProcessId )
{
	errno = 0;
	getpriority(PRIO_PROCESS, ProcessId);
	return errno == 0;
}

bool FLinuxPlatformProcess::IsThisApplicationForeground()
{
	STUBBED("FLinuxPlatformProcess::IsThisApplicationForeground");
	return true;
}

bool FLinuxPlatformProcess::IsApplicationRunning( const TCHAR* ProcName )
{
	FString Commandline = TEXT("pidof '");
	Commandline += ProcName;
	Commandline += TEXT("'  > /dev/null");
	return !system(TCHAR_TO_ANSI(*Commandline));
}

bool FLinuxPlatformProcess::ExecProcess( const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr )
{
	TArray<FString> ArgsArray;
	FString(Params).ParseIntoArray(&ArgsArray, TEXT(" "), true);
	char *args[ArgsArray.Num()];

	for(int i = 0; i < ArgsArray.Num(); i++)
	{
		args[i] = TCHAR_TO_ANSI(*ArgsArray[i]);
	}
	pid_t pid;
	int status;
	int fd_stdout[2], fd_stderr[2];

	if (pipe(fd_stdout) == -1) 
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("Creating fd_stdout pipe failed with errno = %d (%s)"), ErrNo,
			StringCast< TCHAR >(strerror(ErrNo)).Get());
		return false;
	}

	if (pipe(fd_stderr) == -1) 
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("Creating fd_stderr pipe failed with errno = %d (%s)"), ErrNo,
			StringCast< TCHAR >(strerror(ErrNo)).Get());
		return false;
	}

	pid = fork();
	if(pid < 0)
	{
		int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("fork() failed with errno = %d (%s)"), ErrNo,
			StringCast< TCHAR >(strerror(ErrNo)).Get());
		return false;
	}

	if(pid == 0)
	{
		close(fd_stdout[0]);
		close(fd_stderr[0]);
		dup2(fd_stdout[1], 1);
		dup2(fd_stderr[1], 2);
		close(fd_stdout[1]);
		close(fd_stderr[1]);

		// TODO Not sure if we need to look up URL in PATH
		exit(execv(TCHAR_TO_ANSI(URL), args));
	}
	else
	{
		// TODO This might deadlock, should use select. Rewrite me. Also doesn't handle all errors correctly.
		close(fd_stdout[1]);
		close(fd_stderr[1]);
		do
		{
			pid_t wpid = waitpid(pid, &status, 0);
			if (wpid == -1)
			{
				int ErrNo = errno;
				UE_LOG(LogHAL, Fatal, TEXT("waitpid() failed with errno = %d (%s)"), ErrNo,
				StringCast< TCHAR >(strerror(ErrNo)).Get());
				return false;
			}

			if (WIFEXITED(status))
			{
				*OutReturnCode = WEXITSTATUS(status);
			}
			else if (WIFSIGNALED(status))
			{
				*OutReturnCode = WTERMSIG(status);
			}

		} while (!WIFEXITED(status) && !WIFSIGNALED(status));

		while(1)
		{
			char buf[100];
			int size = read(fd_stdout[0], buf, 100);
			if(size)
			{
				if(OutStdErr)
				{
					*OutStdErr += FString(buf);
				}
			}
			else
			{
				break;
			}
		}

		while(1)
		{
			char buf[100];
			int size = read(fd_stderr[0], buf, 100);
			if(size)
			{
				if(OutStdOut)
				{
					*OutStdOut += FString(buf);
				}
			}
			else
			{
				break;
			}
		}
		return true;
	}
}

void FLinuxPlatformProcess::LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms, ELaunchVerb::Type Verb )
{
	// TODO This ignores parms and verb
	pid_t pid = fork();
	if (pid == 0)
	{
		exit(execl("/usr/bin/xdg-open", "xdg-open", TCHAR_TO_ANSI(FileName), (char *)0));
	}
}

void FLinuxPlatformProcess::ExploreFolder( const TCHAR* FilePath )
{
	// TODO This is broken, not an explore action but should be fine if called on a directory
	FLinuxPlatformProcess::LaunchFileInDefaultExternalApplication(FilePath, NULL, ELaunchVerb::Edit);
}
