// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericPlatformMisc.cpp: Generic implementations of misc platform functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "LinuxPlatformMisc.h"
#include "LinuxApplication.h"
#include "LinuxPlatformCrashContext.h"

#include <sys/sysinfo.h>
#include <sched.h>
#include <fcntl.h>
#include <signal.h>

#include "ModuleManager.h"

// define for glibc 2.12.2 and lower (which is shipped with CentOS 6.x and which we target by default)
#define __secure_getenv getenv

namespace PlatformMiscLimits
{
	enum
	{
		MaxUserHomeDirLength= MAX_PATH + 1,
		MaxOsGuidLength = 32,
	};
};

namespace
{
	/**
	 * Empty handler so some signals are just not ignored
	 */
	void EmptyChildHandler(int32 Signal, siginfo_t* Info, void* Context)
	{
	}

	/**
	 * Installs SIGCHLD signal handler so we can wait for our children (otherwise they are reaped automatically)
	 */
	void InstallChildExitedSignalHanlder()
	{
		struct sigaction Action;
		FMemory::Memzero(&Action, sizeof(struct sigaction));
		Action.sa_sigaction = EmptyChildHandler;
		sigemptyset(&Action.sa_mask);
		Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
		sigaction(SIGCHLD, &Action, NULL);
	}
}

bool FLinuxPlatformMisc::ControlScreensaver(EScreenSaverAction Action)
{
	if (Action == FGenericPlatformMisc::EScreenSaverAction::Disable)
	{
		SDL_DisableScreenSaver();
	}
	else
	{
		SDL_EnableScreenSaver();
	}

	return true;
}

const TCHAR* FLinuxPlatformMisc::RootDir()
{
	const TCHAR* TrueRootDir = FGenericPlatformMisc::RootDir();
	return TrueRootDir;
}

void FLinuxPlatformMisc::NormalizePath(FString& InPath)
{
	// only expand if path starts with ~, e.g. ~/ should be expanded, /~ should not
	if (InPath.StartsWith(TEXT("~"), ESearchCase::CaseSensitive))	// case sensitive is quicker, and our substring doesn't care
	{
		static bool bHaveHome = false;
		static TCHAR CachedResult[PlatformMiscLimits::MaxUserHomeDirLength];

		if (!bHaveHome)
		{
			CachedResult[0] = TEXT('~');	// init with a default value that changes nothing
			CachedResult[1] = TEXT('\0');

			//  get user $HOME var first
			const char * VarValue = secure_getenv("HOME");
			if (NULL != VarValue)
			{
				FCString::Strcpy(CachedResult, ARRAY_COUNT(CachedResult) - 1, ANSI_TO_TCHAR(VarValue));
				bHaveHome = true;
			}

			// if var failed
			if (!bHaveHome)
			{
				struct passwd * UserInfo = getpwuid(getuid());
				if (NULL != UserInfo && NULL != UserInfo->pw_dir)
				{
					FCString::Strcpy(CachedResult, ARRAY_COUNT(CachedResult) - 1, ANSI_TO_TCHAR(UserInfo->pw_dir));
					bHaveHome = true;
				}
				else
				{
					// fail for realz
					UE_LOG(LogInit, Fatal, TEXT("Could not get determine user home directory."));
				}
			}
		}

		InPath = InPath.Replace(TEXT("~"), CachedResult, ESearchCase::CaseSensitive);
	}
}

namespace
{
	bool GInitializedSDL = false;
}

void FLinuxPlatformMisc::PlatformInit()
{
	// install a platform-specific signal handler
	InstallChildExitedSignalHanlder();

	UE_LOG(LogInit, Log, TEXT("Linux hardware info:"));
	UE_LOG(LogInit, Log, TEXT(" - this process' id (pid) is %d, parent process' id (ppid) is %d"), static_cast< int32 >(getpid()), static_cast< int32 >(getppid()));
	UE_LOG(LogInit, Log, TEXT(" - we are %srunning under debugger"), IsDebuggerPresent() ? TEXT("") : TEXT("not "));
	UE_LOG(LogInit, Log, TEXT(" - machine network name is '%s'"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT(" - we're logged in %s"), FPlatformMisc::HasBeenStartedRemotely() ? TEXT("remotely") : TEXT("locally"));
	UE_LOG(LogInit, Log, TEXT(" - Number of physical cores available for the process: %d"), FPlatformMisc::NumberOfCores());
	UE_LOG(LogInit, Log, TEXT(" - Number of logical cores available for the process: %d"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	UE_LOG(LogInit, Log, TEXT(" - Memory allocator used: %s"), GMalloc->GetDescriptiveName());

	UE_LOG(LogInit, Log, TEXT("Linux-specific commandline switches:"));
	UE_LOG(LogInit, Log, TEXT(" -%s (currently %s): suppress parsing of DWARF debug info (callstacks will be generated faster, but won't have line numbers)"), 
		TEXT(CMDARG_SUPPRESS_DWARF_PARSING), FParse::Param( FCommandLine::Get(), TEXT(CMDARG_SUPPRESS_DWARF_PARSING)) ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogInit, Log, TEXT(" -ansimalloc - use malloc()/free() from libc (useful for tools like valgrind and electric fence)"));
	UE_LOG(LogInit, Log, TEXT(" -jemalloc - use jemalloc for all memory allocation"));
	UE_LOG(LogInit, Log, TEXT(" -binnedmalloc - use binned malloc  for all memory allocation"));

	// [RCL] FIXME: this should be printed in specific modules, if at all
	UE_LOG(LogInit, Log, TEXT(" -httpproxy=ADDRESS:PORT - redirects HTTP requests to a proxy (only supported if compiled with libcurl)"));
	UE_LOG(LogInit, Log, TEXT(" -reuseconn - allow libcurl to reuse HTTP connections (only matters if compiled with libcurl)"));
	UE_LOG(LogInit, Log, TEXT(" -virtmemkb=NUMBER - sets process virtual memory (address space) limit (overrides VirtualMemoryLimitInKB value from .ini)"));

	UE_LOG(LogInit, Log, TEXT("Setting LC_NUMERIC to en_US"));
	if (setenv("LC_NUMERIC", "en_US", 1) != 0)
	{
		int ErrNo = errno;
		UE_LOG(LogInit, Warning, TEXT("Unable to setenv(): errno=%d (%s)"), ErrNo, ANSI_TO_TCHAR(strerror(ErrNo)));
	}

	// skip for servers and programs, unless they request later
	if (!UE_SERVER && !IS_PROGRAM)
	{
		PlatformInitMultimedia();
	}

	if (FPlatformMisc::HasBeenStartedRemotely())
	{
		// print output immediately
		setvbuf(stdout, NULL, _IONBF, 0);
	}
}

bool FLinuxPlatformMisc::PlatformInitMultimedia()
{
	if (!GInitializedSDL)
	{
		UE_LOG(LogInit, Log, TEXT("Initializing SDL."));

		SDL_SetHint("SDL_VIDEO_X11_REQUIRE_XRANDR", "1");  // workaround for misbuilt SDL libraries on X11.
		if (SDL_Init(SDL_INIT_EVERYTHING | SDL_INIT_NOPARACHUTE) != 0)
		{
			const char * SDLError = SDL_GetError();

			// do not fail at this point, allow caller handle failure
			UE_LOG(LogInit, Warning, TEXT("Could not initialize SDL: %s"), ANSI_TO_TCHAR(SDLError));
			return false;
		}

		// Used to make SDL push SDL_TEXTINPUT events.
		SDL_StartTextInput();

		GInitializedSDL = true;

		// needs to come after GInitializedSDL, otherwise it will recurse here
		// @TODO [RCL] 2014-09-30 - move to FDisplayMetrics itself sometime after 4.5
		if (!UE_BUILD_SHIPPING)
		{
			// dump information about screens for debug
			FDisplayMetrics DisplayMetrics;
			FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
			DisplayMetrics.PrintToLog();
		}
	}

	return true;
}

void FLinuxPlatformMisc::PlatformTearDown()
{
	if (GInitializedSDL)
	{
		UE_LOG(LogInit, Log, TEXT("Tearing down SDL."));
		SDL_Quit();
		GInitializedSDL = false;
	}
}

GenericApplication* FLinuxPlatformMisc::CreateApplication()
{
	return FLinuxApplication::CreateLinuxApplication();
}

void FLinuxPlatformMisc::GetEnvironmentVariable(const TCHAR* InVariableName, TCHAR* Result, int32 ResultLength)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	ANSICHAR *AnsiResult = secure_getenv(TCHAR_TO_ANSI(*VariableName));
	if (AnsiResult)
	{
		wcsncpy(Result, ANSI_TO_TCHAR(AnsiResult), ResultLength);
	}
	else
	{
		*Result = 0;
	}
}

void FLinuxPlatformMisc::SetEnvironmentVar(const TCHAR* InVariableName, const TCHAR* Value)
{
	FString VariableName = InVariableName;
	VariableName.ReplaceInline(TEXT("-"), TEXT("_"));
	if (Value == NULL || Value[0] == TEXT('\0'))
	{
		unsetenv(TCHAR_TO_ANSI(*VariableName));
	}
	else
	{
		setenv(TCHAR_TO_ANSI(*VariableName), TCHAR_TO_ANSI(Value), 1);
	}
}

void FLinuxPlatformMisc::PumpMessages( bool bFromMainLoop )
{
	if( bFromMainLoop )
	{
		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			if( LinuxApplication )
			{
				LinuxApplication->AddPendingEvent( event );
			}
		}
	}
}

uint32 FLinuxPlatformMisc::GetCharKeyMap(uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformMisc::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, false, true);
}

void FLinuxPlatformMisc::LowLevelOutputDebugString(const TCHAR *Message)
{
	static_assert(PLATFORM_USE_LS_SPEC_FOR_WIDECHAR, "Check printf format");
	fprintf(stderr, "%ls", Message);	// there's no good way to implement that really
}

uint32 FLinuxPlatformMisc::GetKeyMap( uint16* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };

	uint32 NumMappings = 0;

	if (KeyCodes && KeyNames && (MaxMappings > 0))
	{
		ADDKEYMAP(SDL_SCANCODE_BACKSPACE, TEXT("BackSpace"));
		ADDKEYMAP(SDL_SCANCODE_TAB, TEXT("Tab"));
		ADDKEYMAP(SDL_SCANCODE_RETURN, TEXT("Enter"));
		ADDKEYMAP(SDL_SCANCODE_RETURN2, TEXT("Enter"));
		ADDKEYMAP(SDL_SCANCODE_KP_ENTER, TEXT("Enter"));
		ADDKEYMAP(SDL_SCANCODE_PAUSE, TEXT("Pause"));

		ADDKEYMAP(SDL_SCANCODE_ESCAPE, TEXT("Escape"));
		ADDKEYMAP(SDL_SCANCODE_SPACE, TEXT("SpaceBar"));
		ADDKEYMAP(SDL_SCANCODE_PAGEUP, TEXT("PageUp"));
		ADDKEYMAP(SDL_SCANCODE_PAGEDOWN, TEXT("PageDown"));
		ADDKEYMAP(SDL_SCANCODE_END, TEXT("End"));
		ADDKEYMAP(SDL_SCANCODE_HOME, TEXT("Home"));

		ADDKEYMAP(SDL_SCANCODE_LEFT, TEXT("Left"));
		ADDKEYMAP(SDL_SCANCODE_UP, TEXT("Up"));
		ADDKEYMAP(SDL_SCANCODE_RIGHT, TEXT("Right"));
		ADDKEYMAP(SDL_SCANCODE_DOWN, TEXT("Down"));

		ADDKEYMAP(SDL_SCANCODE_INSERT, TEXT("Insert"));
		ADDKEYMAP(SDL_SCANCODE_DELETE, TEXT("Delete"));

		ADDKEYMAP(SDL_SCANCODE_F1, TEXT("F1"));
		ADDKEYMAP(SDL_SCANCODE_F2, TEXT("F2"));
		ADDKEYMAP(SDL_SCANCODE_F3, TEXT("F3"));
		ADDKEYMAP(SDL_SCANCODE_F4, TEXT("F4"));
		ADDKEYMAP(SDL_SCANCODE_F5, TEXT("F5"));
		ADDKEYMAP(SDL_SCANCODE_F6, TEXT("F6"));
		ADDKEYMAP(SDL_SCANCODE_F7, TEXT("F7"));
		ADDKEYMAP(SDL_SCANCODE_F8, TEXT("F8"));
		ADDKEYMAP(SDL_SCANCODE_F9, TEXT("F9"));
		ADDKEYMAP(SDL_SCANCODE_F10, TEXT("F10"));
		ADDKEYMAP(SDL_SCANCODE_F11, TEXT("F11"));
		ADDKEYMAP(SDL_SCANCODE_F12, TEXT("F12"));

        ADDKEYMAP(SDL_SCANCODE_CAPSLOCK, TEXT("CapsLock"));
        ADDKEYMAP(SDL_SCANCODE_LCTRL, TEXT("LeftControl"));
        ADDKEYMAP(SDL_SCANCODE_LSHIFT, TEXT("LeftShift"));
        ADDKEYMAP(SDL_SCANCODE_LALT, TEXT("LeftAlt"));
        ADDKEYMAP(SDL_SCANCODE_RCTRL, TEXT("RightControl"));
        ADDKEYMAP(SDL_SCANCODE_RSHIFT, TEXT("RightShift"));
        ADDKEYMAP(SDL_SCANCODE_RALT, TEXT("RightAlt"));
	}

	check(NumMappings < MaxMappings);
	return NumMappings;
}

void FLinuxPlatformMisc::ClipboardCopy(const TCHAR* Str)
{
	if (SDL_HasClipboardText() == SDL_TRUE)
	{
		if (SDL_SetClipboardText(TCHAR_TO_UTF8(Str)))
		{
			UE_LOG(LogInit, Fatal, TEXT("Error copying clipboard contents: %s\n"), ANSI_TO_TCHAR(SDL_GetError()));
		}
	}
}
void FLinuxPlatformMisc::ClipboardPaste(class FString& Result)
{
	char* ClipContent;
	ClipContent = SDL_GetClipboardText();

	if (!ClipContent)
	{
		UE_LOG(LogInit, Fatal, TEXT("Error pasting clipboard contents: %s\n"), ANSI_TO_TCHAR(SDL_GetError()));
		// unreachable
		Result = TEXT("");
	}
	else
	{
		Result = FString(UTF8_TO_TCHAR(ClipContent));
	}
	SDL_free(ClipContent);
}

EAppReturnType::Type FLinuxPlatformMisc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	int NumberOfButtons = 0;

	// if multimedia cannot be initialized for messagebox, just fall back to default implementation
	if (!FPlatformMisc::PlatformInitMultimedia()) //	will not initialize more than once
	{
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
	}

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_VIDEO);
#endif // DO_CHECK

	SDL_MessageBoxButtonData *Buttons = nullptr;

	switch (MsgType)
	{
		case EAppMsgType::Ok:
			NumberOfButtons = 1;
			Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
			Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
			Buttons[0].text = "Ok";
			Buttons[0].buttonid = EAppReturnType::Ok;
			break;

		case EAppMsgType::YesNo:
			NumberOfButtons = 2;
			Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
			Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[0].text = "Yes";
			Buttons[0].buttonid = EAppReturnType::Yes;
			Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[1].text = "No";
			Buttons[1].buttonid = EAppReturnType::No;
			break;

		case EAppMsgType::OkCancel:
			NumberOfButtons = 2;
			Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
			Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[0].text = "Ok";
			Buttons[0].buttonid = EAppReturnType::Ok;
			Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[1].text = "Cancel";
			Buttons[1].buttonid = EAppReturnType::Cancel;
			break;

		case EAppMsgType::YesNoCancel:
			NumberOfButtons = 3;
			Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
			Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[0].text = "Yes";
			Buttons[0].buttonid = EAppReturnType::Yes;
			Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[1].text = "No";
			Buttons[1].buttonid = EAppReturnType::No;
			Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[2].text = "Cancel";
			Buttons[2].buttonid = EAppReturnType::Cancel;
			break;

		case EAppMsgType::CancelRetryContinue:
			NumberOfButtons = 3;
			Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
			Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[0].text = "Continue";
			Buttons[0].buttonid = EAppReturnType::Continue;
			Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[1].text = "Retry";
			Buttons[1].buttonid = EAppReturnType::Retry;
			Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[2].text = "Cancel";
			Buttons[2].buttonid = EAppReturnType::Cancel;
			break;

		case EAppMsgType::YesNoYesAllNoAll:
			NumberOfButtons = 4;
			Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
			Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[0].text = "Yes";
			Buttons[0].buttonid = EAppReturnType::Yes;
			Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[1].text = "No";
			Buttons[1].buttonid = EAppReturnType::No;
			Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[2].text = "Yes to all";
			Buttons[2].buttonid = EAppReturnType::YesAll;
			Buttons[3].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[3].text = "No to all";
			Buttons[3].buttonid = EAppReturnType::NoAll;
			break;

		case EAppMsgType::YesNoYesAllNoAllCancel:
			NumberOfButtons = 5;
			Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
			Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[0].text = "Yes";
			Buttons[0].buttonid = EAppReturnType::Yes;
			Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[1].text = "No";
			Buttons[1].buttonid = EAppReturnType::No;
			Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[2].text = "Yes to all";
			Buttons[2].buttonid = EAppReturnType::YesAll;
			Buttons[3].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[3].text = "No to all";
			Buttons[3].buttonid = EAppReturnType::NoAll;
			Buttons[4].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[4].text = "Cancel";
			Buttons[4].buttonid = EAppReturnType::Cancel;
			break;

		case EAppMsgType::YesNoYesAll:
			NumberOfButtons = 3;
			Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
			Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[0].text = "Yes";
			Buttons[0].buttonid = EAppReturnType::Yes;
			Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[1].text = "No";
			Buttons[1].buttonid = EAppReturnType::No;
			Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
			Buttons[2].text = "Yes to all";
			Buttons[2].buttonid = EAppReturnType::YesAll;
			break;
	}

	FTCHARToUTF8 CaptionUTF8(Caption);
	FTCHARToUTF8 TextUTF8(Text);
	SDL_MessageBoxData MessageBoxData = 
	{
		SDL_MESSAGEBOX_INFORMATION,
		NULL, // No parent window
		CaptionUTF8.Get(),
		TextUTF8.Get(),
		NumberOfButtons,
		Buttons,
		NULL // Default color scheme
	};

	int ButtonPressed = -1;
	if (SDL_ShowMessageBox(&MessageBoxData, &ButtonPressed) == -1) 
	{
		UE_LOG(LogInit, Fatal, TEXT("Error Presenting MessageBox: %s\n"), ANSI_TO_TCHAR(SDL_GetError()));
		// unreachable
		return EAppReturnType::Cancel;
	}

	delete[] Buttons;

	return ButtonPressed == -1 ? EAppReturnType::Cancel : static_cast<EAppReturnType::Type>(ButtonPressed);
}

int32 FLinuxPlatformMisc::NumberOfCores()
{
	// WARNING: this function ignores edge cases like affinity mask changes (and even more fringe cases like CPUs going offline)
	// in the name of performance (higher level code calls NumberOfCores() way too often...)
	static int32 NumCoreIds = 0;
	if (NumCoreIds == 0)
	{
		cpu_set_t AvailableCpusMask;
		CPU_ZERO(&AvailableCpusMask);

		if (0 != sched_getaffinity(0, sizeof(AvailableCpusMask), &AvailableCpusMask))
		{
			NumCoreIds = 1;	// we are running on something, right?
		}
		else
		{
			char FileNameBuffer[1024];
			unsigned char PossibleCores[CPU_SETSIZE] = { 0 };

			for(int32 CpuIdx = 0; CpuIdx < CPU_SETSIZE; ++CpuIdx)
			{
				if (CPU_ISSET(CpuIdx, &AvailableCpusMask))
				{
					sprintf(FileNameBuffer, "/sys/devices/system/cpu/cpu%d/topology/core_id", CpuIdx);
					
					FILE* CoreIdFile = fopen(FileNameBuffer, "r");
					unsigned int CoreId = 0;
					if (CoreIdFile)
					{
						if (1 != fscanf(CoreIdFile, "%d", &CoreId))
						{
							CoreId = 0;
						}
						fclose(CoreIdFile);
					}

					if (CoreId >= ARRAY_COUNT(PossibleCores))
					{
						CoreId = 0;
					}
					
					PossibleCores[ CoreId ] = 1;
				}
			}

			for(int32 Idx = 0; Idx < ARRAY_COUNT(PossibleCores); ++Idx)
			{
				NumCoreIds += PossibleCores[Idx];
			}
		}
	}

	return NumCoreIds;
}

int32 FLinuxPlatformMisc::NumberOfCoresIncludingHyperthreads()
{
	// WARNING: this function ignores edge cases like affinity mask changes (and even more fringe cases like CPUs going offline)
	// in the name of performance (higher level code calls NumberOfCores() way too often...)
	static int32 NumCoreIds = 0;
	if (NumCoreIds == 0)
	{
		cpu_set_t AvailableCpusMask;
		CPU_ZERO(&AvailableCpusMask);

		if (0 != sched_getaffinity(0, sizeof(AvailableCpusMask), &AvailableCpusMask))
		{
			NumCoreIds = 1;	// we are running on something, right?
		}
		else
		{
			return CPU_COUNT(&AvailableCpusMask);
		}
	}

	return NumCoreIds;
}

void FLinuxPlatformMisc::LoadPreInitModules()
{
#if WITH_EDITOR
	FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
#endif // WITH_EDITOR
}

void FLinuxPlatformMisc::LoadStartupModules()
{
#if !IS_PROGRAM && !UE_SERVER
	FModuleManager::Get().LoadModule(TEXT("ALAudio"));	// added in Launch.Build.cs for non-server targets
	FModuleManager::Get().LoadModule(TEXT("HeadMountedDisplay"));
#endif // !IS_PROGRAM && !UE_SERVER

#if WITH_STEAMCONTROLLER
	FModuleManager::Get().LoadModule(TEXT("SteamController"));
#endif // WITH_STEAMCONTROLLER

#if WITH_EDITOR
	FModuleManager::Get().LoadModule(TEXT("SourceCodeAccess"));
#endif	//WITH_EDITOR
}

const TCHAR* FLinuxPlatformMisc::GetNullRHIShaderFormat()
{
	return TEXT("GLSL_150");
}

#if PLATFORM_HAS_CPUID
FString FLinuxPlatformMisc::GetCPUVendor()
{
	union
	{
		char Buffer[12+1];
		struct
		{
			int dw0;
			int dw1;
			int dw2;
		} Dw;
	} VendorResult;


	int32 Args[4];
	asm( "cpuid" : "=a" (Args[0]), "=b" (Args[1]), "=c" (Args[2]), "=d" (Args[3]) : "a" (0));

	VendorResult.Dw.dw0 = Args[1];
	VendorResult.Dw.dw1 = Args[3];
	VendorResult.Dw.dw2 = Args[2];
	VendorResult.Buffer[12] = 0;

	return ANSI_TO_TCHAR(VendorResult.Buffer);
}

uint32 FLinuxPlatformMisc::GetCPUInfo()
{
	uint32 Args[4];
	asm( "cpuid" : "=a" (Args[0]), "=b" (Args[1]), "=c" (Args[2]), "=d" (Args[3]) : "a" (1));

	return Args[0];
}
#endif // PLATFORM_HAS_CPUID

#if !UE_BUILD_SHIPPING
bool FLinuxPlatformMisc::IsDebuggerPresent()
{
	// If a process is tracing this one then TracerPid in /proc/self/status will
	// be the id of the tracing process. Use SignalHandler safe functions 

	int StatusFile = open("/proc/self/status", O_RDONLY);
	if (StatusFile == -1) 
	{
		// Failed - unknown debugger status.
		return false;
	}

	char Buffer[256];	
	ssize_t Length = read(StatusFile, Buffer, sizeof(Buffer));
	
	bool bDebugging = false;
	const char* TracerString = "TracerPid:\t";
	const ssize_t LenTracerString = strlen(TracerString);
	int i = 0;

	while((Length - i) > LenTracerString)
	{
		// TracerPid is found
		if (strncmp(&Buffer[i], TracerString, LenTracerString) == 0)
		{
			// 0 if no process is tracing.
			bDebugging = Buffer[i + LenTracerString] != '0';
			break;
		}

		++i;
	}

	close(StatusFile);
	return bDebugging;
}
#endif // !UE_BUILD_SHIPPING

bool FLinuxPlatformMisc::HasBeenStartedRemotely()
{
	static bool bHaveAnswer = false;
	static bool bResult = false;

	if (!bHaveAnswer)
	{
		const char * VarValue = secure_getenv("SSH_CONNECTION");
		bResult = (VarValue && strlen(VarValue) != 0);
		bHaveAnswer = true;
	}

	return bResult;
}

FString FLinuxPlatformMisc::GetOperatingSystemId()
{
	static bool bHasCachedResult = false;
	static FString CachedResult;

	if (!bHasCachedResult)
	{
		int OsGuidFile = open("/etc/machine-id", O_RDONLY);
		if (OsGuidFile != -1)
		{
			char Buffer[PlatformMiscLimits::MaxOsGuidLength + 1] = {0};
			ssize_t ReadBytes = read(OsGuidFile, Buffer, sizeof(Buffer) - 1);

			if (ReadBytes > 0)
			{
				CachedResult = ANSI_TO_TCHAR(Buffer);
			}

			close(OsGuidFile);
		}

		// old POSIX gethostid() is not useful. It is impossible to have globally unique 32-bit GUIDs and most
		// systems don't try hard implementing it these days (glibc will return a permuted IP address, often 127.0.0.1)
		// Due to that, we just ignore that call and consider lack of systemd's /etc/machine-id a failure to obtain the host id.

		bHasCachedResult = true;	// even if we failed to read the real one
	}

	return CachedResult;
}
