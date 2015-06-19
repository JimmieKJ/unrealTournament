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
	void *Handle = dlopen( TCHAR_TO_ANSI(*AbsolutePath), RTLD_LAZY | RTLD_GLOBAL );
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
			Output.SetNumUninitialized(BytesAvailable);
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
	UE_LOG(LogHAL, Verbose, TEXT("FLinuxPlatformProcess::LaunchURL: '%s'"), URL);
	if (pid == 0)
	{
		exit(execl("/usr/bin/xdg-open", "xdg-open", TCHAR_TO_ANSI(URL), (char *)0));
	}
}

/**
 * This class exists as an imperfect workaround to allow both "fire and forget" children and children about whose return code we actually care.
 * (maybe we could fork and daemonize ourselves for the first case instead?)
 */
struct FChildWaiterThread : public FRunnable
{
	/** Global table of all waiter threads */
	static TArray<FChildWaiterThread *>		ChildWaiterThreadsArray;

	/** Lock guarding the acess to child waiter threads */
	static FCriticalSection					ChildWaiterThreadsArrayGuard;

	/** Pid of child to wait for */
	int ChildPid;

	FChildWaiterThread(pid_t InChildPid)
		:	ChildPid(InChildPid)
	{
		// add ourselves to thread array
		ChildWaiterThreadsArrayGuard.Lock();
		ChildWaiterThreadsArray.Add(this);
		ChildWaiterThreadsArrayGuard.Unlock();
	}

	virtual ~FChildWaiterThread()
	{
		// remove
		ChildWaiterThreadsArrayGuard.Lock();
		ChildWaiterThreadsArray.RemoveSingle(this);
		ChildWaiterThreadsArrayGuard.Unlock();
	}

	virtual uint32 Run()
	{
		for(;;)	// infinite loop in case we get EINTR and have to repeat
		{
			siginfo_t SignalInfo;
			if (waitid(P_PID, ChildPid, &SignalInfo, WEXITED))
			{
				if (errno != EINTR)
				{
					int ErrNo = errno;
					UE_LOG(LogHAL, Fatal, TEXT("FChildWaiterThread::Run(): waitid for pid %d failed (errno=%d, %s)"), 
						   static_cast< int32 >(ChildPid), ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
					break;	// exit the loop if for some reason Fatal log (above) returns
				}
			}
			else
			{
				check(SignalInfo.si_pid == ChildPid);
				break;
			}
		}

		return 0;
	}

	virtual void Exit()
	{
		// unregister from the array
		delete this;
	}
};

/** See FChildWaiterThread */
TArray<FChildWaiterThread *> FChildWaiterThread::ChildWaiterThreadsArray;
/** See FChildWaiterThread */
FCriticalSection FChildWaiterThread::ChildWaiterThreadsArrayGuard;

FProcHandle FLinuxPlatformProcess::CreateProc(const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWrite)
{
	// @TODO bLaunchHidden bLaunchReallyHidden are not handled
	// We need an absolute path to executable
	FString ProcessPath = URL;
	if (*URL != TEXT('/'))
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
	int Argc = Commandline.ParseIntoArray(ArgvArray, TEXT(" "), true);
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

	int PosixSpawnErrNo = posix_spawn(&ChildPid, TCHAR_TO_ANSI(*ProcessPath), &FileActions, nullptr, Argv, environ);
	posix_spawn_file_actions_destroy(&FileActions);
	if (PosixSpawnErrNo != 0)
	{
		UE_LOG(LogHAL, Fatal, TEXT("FLinuxPlatformProcess::CreateProc: posix_spawn() failed (%d, %s)"), PosixSpawnErrNo, ANSI_TO_TCHAR(strerror(PosixSpawnErrNo)));
		return FProcHandle();	// produce knowingly invalid handle if for some reason Fatal log (above) returns
	}

	// renice the child (subject to race condition).
	// Why this instead of posix_spawn_setschedparam()? 
	// Because posix_spawnattr priority is unusable under Linux due to min/max priority range being [0;0] for the default scheduler
	if (PriorityModifier != 0)
	{
		errno = 0;
		int TheirCurrentPrio = getpriority(PRIO_PROCESS, ChildPid);

		if (errno != 0)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("FLinuxPlatformProcess::CreateProc: could not get child's priority, errno=%d (%s)"),
				ErrNo,
				ANSI_TO_TCHAR(strerror(ErrNo))
			);
			
			// proceed anyway...
			TheirCurrentPrio = 0;
		}

		rlimit PrioLimits;
		int MaxPrio = 0;
		if (getrlimit(RLIMIT_NICE, &PrioLimits) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("FLinuxPlatformProcess::CreateProc: could not get priority limits (RLIMIT_NICE), errno=%d (%s)"),
				ErrNo,
				ANSI_TO_TCHAR(strerror(ErrNo))
			);

			// proceed anyway...
		}
		else
		{
			MaxPrio = PrioLimits.rlim_cur;
		}

		int NewPrio = TheirCurrentPrio;
		if (PriorityModifier > 0)
		{
			// decrease the nice value - will perhaps fail, it's up to the user to run with proper permissions
			NewPrio -= 10;
		}
		else
		{
			NewPrio += 10;
		}

		// cap to [RLIMIT_NICE, 19]
		NewPrio = FMath::Min(19, NewPrio);
		NewPrio = FMath::Max(MaxPrio, NewPrio);	// MaxPrio is actually the _lowest_ numerically priority

		if (setpriority(PRIO_PROCESS, ChildPid, NewPrio) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Warning, TEXT("FLinuxPlatformProcess::CreateProc: could not change child's priority (nice value) from %d to %d, errno=%d (%s)"),
				TheirCurrentPrio, NewPrio,
				ErrNo,
				ANSI_TO_TCHAR(strerror(ErrNo))
			);
		}
		else
		{
			UE_LOG(LogHAL, Log, TEXT("Changed child's priority (nice value) to %d (change from %d)"), NewPrio, TheirCurrentPrio);
		}
	}

	else
	{
		UE_LOG(LogHAL, Log, TEXT("FLinuxPlatformProcess::CreateProc: spawned child %d"), ChildPid);
	}

	if (OutProcessID)
	{
		*OutProcessID = ChildPid;
	}

	// [RCL] 2015-03-11 @FIXME: is bLaunchDetached usable when determining whether we're in 'fire and forget' mode? This doesn't exactly match what bLaunchDetached is used for.
	return FProcHandle(new FProcState(ChildPid, bLaunchDetached));
}

/** Initialization constructor. */
FProcState::FProcState(pid_t InProcessId, bool bInFireAndForget)
	:	ProcessId(InProcessId)
	,	bIsRunning(true)  // assume it is
	,	bHasBeenWaitedFor(false)
	,	ReturnCode(-1)
	,	bFireAndForget(bInFireAndForget)
{
}

FProcState::~FProcState()
{
	if (!bFireAndForget)
	{
		// If not in 'fire and forget' mode, try to catch the common problems that leave zombies:
		// - We don't want to close the handle of a running process as with our current scheme this will certainly leak a zombie.
		// - Nor we want to leave the handle unwait()ed for.
		
		if (bIsRunning)
		{
			// Warn the users before going into what may be a very long block
			UE_LOG(LogHAL, Warning, TEXT("Closing a process handle while the process (pid=%d) is still running - we will block until it exits to prevent a zombie"),
				GetProcessId()
			);
		}
		else if (!bHasBeenWaitedFor)	// if child is not running, but has not been waited for, still communicate a problem, but we shouldn't be blocked for long in this case.
		{
			UE_LOG(LogHAL, Warning, TEXT("Closing a process handle of a process (pid=%d) that has not been wait()ed for - will wait() now to reap a zombie"),
				GetProcessId()
			);
		}

		Wait();	// will exit immediately if everything is Ok
	}
	else if (IsRunning())
	{
		// warn about leaking a thread ;/
		UE_LOG(LogHAL, Warning, TEXT("Process (pid=%d) is still running - we will reap it in a waiter thread, but the thread handle is going to be leaked."),
			   GetProcessId()
			);

		FChildWaiterThread * WaiterRunnable = new FChildWaiterThread(GetProcessId());
		// [RCL] 2015-03-11 @FIXME: do not leak
		FRunnableThread * WaiterThread = FRunnableThread::Create(WaiterRunnable, *FString::Printf(TEXT("waitpid(%d)"), GetProcessId(), 32768 /* needs just a small stack */, TPri_BelowNormal));
	}
}

bool FProcState::IsRunning()
{
	if (bIsRunning)
	{
		check(!bHasBeenWaitedFor);	// check for the sake of internal consistency

		// check if actually running
		int KillResult = kill(GetProcessId(), 0);	// no actual signal is sent
		check(KillResult != -1 || errno != EINVAL);

		bIsRunning = (KillResult == 0 || (KillResult == -1 && errno == EPERM));

		// additional check if it's a zombie
		if (bIsRunning)
		{
			for(;;)	// infinite loop in case we get EINTR and have to repeat
			{
				siginfo_t SignalInfo;
				SignalInfo.si_pid = 0;	// if remains 0, treat as child was not waitable (i.e. was running)
				if (waitid(P_PID, GetProcessId(), &SignalInfo, WEXITED | WNOHANG | WNOWAIT))
				{
					if (errno != EINTR)
					{
						int ErrNo = errno;
						UE_LOG(LogHAL, Fatal, TEXT("FLinuxPlatformProcess::WaitForProc: waitid for pid %d failed (errno=%d, %s)"), 
							static_cast< int32 >(GetProcessId()), ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
						break;	// exit the loop if for some reason Fatal log (above) returns
					}
				}
				else
				{
					bIsRunning = ( SignalInfo.si_pid != GetProcessId() );
					break;
				}
			}
		}

		// If child is a zombie, wait() immediately to free up kernel resources. Higher level code
		// (e.g. shader compiling manager) can hold on to handle of no longer running process for longer,
		// which is a dubious, but valid behavior. We don't want to keep zombie around though.
		if (!bIsRunning)
		{
			UE_LOG(LogHAL, Log, TEXT("Child %d is no more running (zombie), Wait()ing immediately."), GetProcessId() );
			Wait();
		}
	}

	return bIsRunning;
}

bool FProcState::GetReturnCode(int32* ReturnCodePtr)
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

void FProcState::Wait()
{
	if (bHasBeenWaitedFor)
	{
		UE_LOG(LogHAL, Log, TEXT("FLinuxPlatformProcess::WaitForProc: already waited on process %d"), static_cast<int32>(GetProcessId()));
		return;	// we could try waitpid() another time, but why
	}

	for(;;)	// infinite loop in case we get EINTR and have to repeat
	{
		siginfo_t SignalInfo;
		if (waitid(P_PID, GetProcessId(), &SignalInfo, WEXITED))
		{
			if (errno != EINTR)
			{
				int ErrNo = errno;
				UE_LOG(LogHAL, Fatal, TEXT("FLinuxPlatformProcess::WaitForProc: waitid for pid %d failed (errno=%d, %s)"), 
					static_cast< int32 >(GetProcessId()), ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
				break;	// exit the loop if for some reason Fatal log (above) returns
			}
		}
		else
		{
			check(SignalInfo.si_pid == GetProcessId());

			ReturnCode = (SignalInfo.si_code == CLD_EXITED) ? SignalInfo.si_status : -2;

			UE_LOG(LogHAL, Log, TEXT("FLinuxPlatformProcess::WaitForProc: process %d waited, SignalCode:%d ReturnCode:%d"), static_cast<int32>(GetProcessId()), SignalInfo.si_code, ReturnCode);

			bHasBeenWaitedFor = true;
			bIsRunning = false;	// set in advance
			break;
		}
	}
}

bool FLinuxPlatformProcess::IsProcRunning( FProcHandle & ProcessHandle )
{
	FProcState * ProcInfo = ProcessHandle.GetProcessInfo();
	return ProcInfo ? ProcInfo->IsRunning() : false;
}

void FLinuxPlatformProcess::WaitForProc( FProcHandle & ProcessHandle )
{
	FProcState * ProcInfo = ProcessHandle.GetProcessInfo();
	if (ProcInfo)
	{
		ProcInfo->Wait();
	}
}

void FLinuxPlatformProcess::CloseProc(FProcHandle & ProcessHandle)
{
	// dispose of both handle and process info
	FProcState * ProcInfo = ProcessHandle.GetProcessInfo();
	ProcessHandle.Reset();

	delete ProcInfo;
}

void FLinuxPlatformProcess::TerminateProc( FProcHandle & ProcessHandle, bool KillTree )
{
	if (KillTree)
	{
		// TODO: enumerate the children
		STUBBED("FLinuxPlatformProcess::TerminateProc() : Killing a subtree is not implemented yet");
	}

	FProcState * ProcInfo = ProcessHandle.GetProcessInfo();
	if (ProcInfo)
	{
		int KillResult = kill(ProcInfo->GetProcessId(), SIGTERM);	// graceful
		check(KillResult != -1 || errno != EINVAL);
	}
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

	FProcState * ProcInfo = ProcHandle.GetProcessInfo();
	return ProcInfo ? ProcInfo->GetReturnCode(ReturnCode) : false;
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
	FString CmdLineParams = Params;
	FString ExecutableFileName = URL;
	int32 ReturnCode = -1;
	FString DefaultError;
	if (!OutStdErr)
	{
		OutStdErr = &DefaultError;
	}

	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;
	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

	bool bInvoked = false;

	const bool bLaunchDetached = true;
	const bool bLaunchHidden = false;
	const bool bLaunchReallyHidden = bLaunchHidden;

	FProcHandle ProcHandle = FPlatformProcess::CreateProc(*ExecutableFileName, *CmdLineParams, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, NULL, 0, NULL, PipeWrite);
	if (ProcHandle.IsValid())
	{
		while (FPlatformProcess::IsProcRunning(ProcHandle))
		{
			FString NewLine = FPlatformProcess::ReadPipe(PipeRead);
			if (NewLine.Len() > 0)
			{
				if (OutStdOut != nullptr)
				{
					*OutStdOut += NewLine;
				}
			}
			FPlatformProcess::Sleep(0.5);
		}

		// read the remainder
		for(;;)
		{
			FString NewLine = FPlatformProcess::ReadPipe(PipeRead);
			if (NewLine.Len() <= 0)
			{
				break;
			}

			if (OutStdOut != nullptr)
			{
				*OutStdOut += NewLine;
			}
		}

		FPlatformProcess::Sleep(0.5);

		bInvoked = true;
		bool bGotReturnCode = FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
		check(bGotReturnCode);
		*OutReturnCode = ReturnCode;

		FPlatformProcess::CloseProc(ProcHandle);
	}
	else
	{
		bInvoked = false;
		*OutReturnCode = -1;
		*OutStdOut = "";
		UE_LOG(LogHAL, Warning, TEXT("Failed to launch Tool. (%s)"), *ExecutableFileName);
	}
	FPlatformProcess::ClosePipe(PipeRead, PipeWrite);
	return bInvoked;
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
