// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericPlatformMisc.cpp: Generic implementations of misc platform functions
=============================================================================*/

#include "CorePrivatePCH.h"
#include "LinuxPlatformMisc.h"
#include "LinuxApplication.h"
#include "LinuxPlatformCrashContext.h"

#include <cpuid.h>
#include <sys/sysinfo.h>
#include <sched.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/vfs.h>	// statfs()
#include <sys/ioctl.h>

#include <ifaddrs.h>	// ethernet mac
#include <net/if.h>
#include <net/if_arp.h>

#include "ModuleManager.h"
#include "HAL/ThreadHeartBeat.h"

// define for glibc 2.12.2 and lower (which is shipped with CentOS 6.x and which we target by default)
#define __secure_getenv getenv

namespace PlatformMiscLimits
{
	enum
	{
		MaxOsGuidLength = 32
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
		FMemory::Memzero(Action);
		Action.sa_sigaction = EmptyChildHandler;
		sigfillset(&Action.sa_mask);
		Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
		sigaction(SIGCHLD, &Action, nullptr);
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
		InPath = InPath.Replace(TEXT("~"), FPlatformProcess::UserHomeDir(), ESearchCase::CaseSensitive);
	}
}

namespace
{
	bool GInitializedSDL = false;
}

size_t GCacheLineSize = PLATFORM_CACHE_LINE_SIZE;

void LinuxPlatform_UpdateCacheLineSize()
{
	// sysfs "API", as usual ;/
	FILE * SysFsFile = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
	if (SysFsFile)
	{
		int SystemLineSize = 0;
		fscanf(SysFsFile, "%d", &SystemLineSize);
		fclose(SysFsFile);

		if (SystemLineSize > 0)
		{
			GCacheLineSize = SystemLineSize;
		}
	}
}

void FLinuxPlatformMisc::PlatformInit()
{
	// install a platform-specific signal handler
	InstallChildExitedSignalHanlder();

	// do not remove the below check for IsFirstInstance() - it is not just for logging, it actually lays the claim to be first
	bool bFirstInstance = FPlatformProcess::IsFirstInstance();
	bool bIsNullRHI = !FApp::CanEverRender();

	UE_LOG(LogInit, Log, TEXT("Linux hardware info:"));
	UE_LOG(LogInit, Log, TEXT(" - we are %sthe first instance of this executable"), bFirstInstance ? TEXT("") : TEXT("not "));
	UE_LOG(LogInit, Log, TEXT(" - this process' id (pid) is %d, parent process' id (ppid) is %d"), static_cast< int32 >(getpid()), static_cast< int32 >(getppid()));
	UE_LOG(LogInit, Log, TEXT(" - we are %srunning under debugger"), IsDebuggerPresent() ? TEXT("") : TEXT("not "));
	UE_LOG(LogInit, Log, TEXT(" - machine network name is '%s'"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT(" - user name is '%s' (%s)"), FPlatformProcess::UserName(), FPlatformProcess::UserName(false));
	UE_LOG(LogInit, Log, TEXT(" - we're logged in %s"), FPlatformMisc::HasBeenStartedRemotely() ? TEXT("remotely") : TEXT("locally"));
	UE_LOG(LogInit, Log, TEXT(" - we're running %s rendering"), bIsNullRHI ? TEXT("without") : TEXT("with"));
	UE_LOG(LogInit, Log, TEXT(" - CPU: %s '%s' (signature: 0x%X)"), *FPlatformMisc::GetCPUVendor(), *FPlatformMisc::GetCPUBrand(), FPlatformMisc::GetCPUInfo());
	UE_LOG(LogInit, Log, TEXT(" - Number of physical cores available for the process: %d"), FPlatformMisc::NumberOfCores());
	UE_LOG(LogInit, Log, TEXT(" - Number of logical cores available for the process: %d"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	LinuxPlatform_UpdateCacheLineSize();
	UE_LOG(LogInit, Log, TEXT(" - Cache line size: %Zu"), GCacheLineSize);
	UE_LOG(LogInit, Log, TEXT(" - Memory allocator used: %s"), GMalloc->GetDescriptiveName());

	// programs don't need it by default
	if (!IS_PROGRAM || FParse::Param(FCommandLine::Get(), TEXT("calibrateclock")))
	{
		FPlatformTime::CalibrateClock();
	}

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

	// skip for servers and programs, unless they request later
	if (!IS_PROGRAM && !bIsNullRHI)
	{
		PlatformInitMultimedia();
	}

	if (FPlatformMisc::HasBeenStartedRemotely() || FPlatformMisc::IsDebuggerPresent())
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
			UE_LOG(LogInit, Warning, TEXT("Could not initialize SDL: %s"), UTF8_TO_TCHAR(SDLError));
			return false;
		}

		// print out version information
		SDL_version CompileTimeSDLVersion;
		SDL_version RunTimeSDLVersion;
		SDL_VERSION(&CompileTimeSDLVersion);
		SDL_GetVersion(&RunTimeSDLVersion);
		UE_LOG(LogInit, Log, TEXT("Initialized SDL %d.%d.%d (compiled against %d.%d.%d)"),
			CompileTimeSDLVersion.major, CompileTimeSDLVersion.minor, CompileTimeSDLVersion.patch,
			RunTimeSDLVersion.major, RunTimeSDLVersion.minor, RunTimeSDLVersion.patch
			);

		// Used to make SDL push SDL_TEXTINPUT events.
		SDL_StartTextInput();

		GInitializedSDL = true;

		// needs to come after GInitializedSDL, otherwise it will recurse here
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

	FPlatformProcess::CeaseBeingFirstInstance();
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
		wcsncpy(Result, UTF8_TO_TCHAR(AnsiResult), ResultLength);
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
	if (GInitializedSDL && bFromMainLoop)
	{
		if( LinuxApplication )
		{
			LinuxApplication->SaveWindowLocationsForEventLoop();

			SDL_Event event;

			while (SDL_PollEvent(&event))
			{
				LinuxApplication->AddPendingEvent( event );
			}

			LinuxApplication->ClearWindowLocationsAfterEventLoop();
		}
		else
		{
			// No application to send events to. Just flush out the
			// queue.
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				// noop
			}
		}
	}
}

uint32 FLinuxPlatformMisc::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformMisc::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, false, true);
}

void FLinuxPlatformMisc::LowLevelOutputDebugString(const TCHAR *Message)
{
	static_assert(PLATFORM_USE_LS_SPEC_FOR_WIDECHAR, "Check printf format");
	fprintf(stderr, "%ls", Message);	// there's no good way to implement that really
}

uint32 FLinuxPlatformMisc::GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };

	uint32 NumMappings = 0;

	if (KeyCodes && KeyNames && (MaxMappings > 0))
	{
		ADDKEYMAP(SDLK_BACKSPACE, TEXT("BackSpace"));
		ADDKEYMAP(SDLK_TAB, TEXT("Tab"));
		ADDKEYMAP(SDLK_RETURN, TEXT("Enter"));
		ADDKEYMAP(SDLK_RETURN2, TEXT("Enter"));
		ADDKEYMAP(SDLK_KP_ENTER, TEXT("Enter"));
		ADDKEYMAP(SDLK_PAUSE, TEXT("Pause"));

		ADDKEYMAP(SDLK_ESCAPE, TEXT("Escape"));
		ADDKEYMAP(SDLK_SPACE, TEXT("SpaceBar"));
		ADDKEYMAP(SDLK_PAGEUP, TEXT("PageUp"));
		ADDKEYMAP(SDLK_PAGEDOWN, TEXT("PageDown"));
		ADDKEYMAP(SDLK_END, TEXT("End"));
		ADDKEYMAP(SDLK_HOME, TEXT("Home"));

		ADDKEYMAP(SDLK_LEFT, TEXT("Left"));
		ADDKEYMAP(SDLK_UP, TEXT("Up"));
		ADDKEYMAP(SDLK_RIGHT, TEXT("Right"));
		ADDKEYMAP(SDLK_DOWN, TEXT("Down"));

		ADDKEYMAP(SDLK_INSERT, TEXT("Insert"));
		ADDKEYMAP(SDLK_DELETE, TEXT("Delete"));

		ADDKEYMAP(SDLK_F1, TEXT("F1"));
		ADDKEYMAP(SDLK_F2, TEXT("F2"));
		ADDKEYMAP(SDLK_F3, TEXT("F3"));
		ADDKEYMAP(SDLK_F4, TEXT("F4"));
		ADDKEYMAP(SDLK_F5, TEXT("F5"));
		ADDKEYMAP(SDLK_F6, TEXT("F6"));
		ADDKEYMAP(SDLK_F7, TEXT("F7"));
		ADDKEYMAP(SDLK_F8, TEXT("F8"));
		ADDKEYMAP(SDLK_F9, TEXT("F9"));
		ADDKEYMAP(SDLK_F10, TEXT("F10"));
		ADDKEYMAP(SDLK_F11, TEXT("F11"));
		ADDKEYMAP(SDLK_F12, TEXT("F12"));

		ADDKEYMAP(SDLK_LCTRL, TEXT("LeftControl"));
		ADDKEYMAP(SDLK_LSHIFT, TEXT("LeftShift"));
		ADDKEYMAP(SDLK_LALT, TEXT("LeftAlt"));
		ADDKEYMAP(SDLK_LGUI, TEXT("LeftCommand"));
		ADDKEYMAP(SDLK_RCTRL, TEXT("RightControl"));
		ADDKEYMAP(SDLK_RSHIFT, TEXT("RightShift"));
		ADDKEYMAP(SDLK_RALT, TEXT("RightAlt"));
		ADDKEYMAP(SDLK_RGUI, TEXT("RightCommand"));

		ADDKEYMAP(SDLK_KP_0, TEXT("NumPadZero"));
		ADDKEYMAP(SDLK_KP_1, TEXT("NumPadOne"));
		ADDKEYMAP(SDLK_KP_2, TEXT("NumPadTwo"));
		ADDKEYMAP(SDLK_KP_3, TEXT("NumPadThree"));
		ADDKEYMAP(SDLK_KP_4, TEXT("NumPadFour"));
		ADDKEYMAP(SDLK_KP_5, TEXT("NumPadFive"));
		ADDKEYMAP(SDLK_KP_6, TEXT("NumPadSix"));
		ADDKEYMAP(SDLK_KP_7, TEXT("NumPadSeven"));
		ADDKEYMAP(SDLK_KP_8, TEXT("NumPadEight"));
		ADDKEYMAP(SDLK_KP_9, TEXT("NumPadNine"));
		ADDKEYMAP(SDLK_KP_MULTIPLY, TEXT("Multiply"));
		ADDKEYMAP(SDLK_KP_PLUS, TEXT("Add"));
		ADDKEYMAP(SDLK_KP_MINUS, TEXT("Subtract"));
		ADDKEYMAP(SDLK_KP_DECIMAL, TEXT("Decimal"));
		ADDKEYMAP(SDLK_KP_DIVIDE, TEXT("Divide"));

		ADDKEYMAP(SDLK_CAPSLOCK, TEXT("CapsLock"));
		ADDKEYMAP(SDLK_NUMLOCKCLEAR, TEXT("NumLock"));
		ADDKEYMAP(SDLK_SCROLLLOCK, TEXT("ScrollLock"));
	}

	check(NumMappings < MaxMappings);
	return NumMappings;
}

uint8 GOverriddenReturnCode = 0;
bool GHasOverriddenReturnCode = false;

void FLinuxPlatformMisc::RequestExitWithStatus(bool Force, uint8 ReturnCode)
{
	UE_LOG(LogLinux, Log, TEXT("FLinuxPlatformMisc::RequestExit(bForce=%s, ReturnCode=%d)"), Force ? TEXT("true") : TEXT("false"), ReturnCode);

	GOverriddenReturnCode = ReturnCode;
	GHasOverriddenReturnCode = true;

	return FPlatformMisc::RequestExit(Force);
}

bool FLinuxPlatformMisc::HasOverriddenReturnCode(uint8 * OverriddenReturnCodeToUsePtr)
{
	if (GHasOverriddenReturnCode && OverriddenReturnCodeToUsePtr != nullptr)
	{
		*OverriddenReturnCodeToUsePtr = GOverriddenReturnCode;
	}

	return GHasOverriddenReturnCode;
}

const TCHAR* FLinuxPlatformMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	check(OutBuffer && BufferCount);
	if (Error == 0)
	{
		Error = errno;
	}

	FString Message = FString::Printf(TEXT("errno=%d (%s)"), Error, UTF8_TO_TCHAR(strerror(Error)));
	FCString::Strncpy(OutBuffer, *Message, BufferCount);

	return OutBuffer;
}

void FLinuxPlatformMisc::ClipboardCopy(const TCHAR* Str)
{
	if (SDL_HasClipboardText() == SDL_TRUE)
	{
		if (SDL_SetClipboardText(TCHAR_TO_UTF8(Str)))
		{
			UE_LOG(LogInit, Fatal, TEXT("Error copying clipboard contents: %s\n"), UTF8_TO_TCHAR(SDL_GetError()));
		}
	}
}
void FLinuxPlatformMisc::ClipboardPaste(class FString& Result)
{
	char* ClipContent;
	ClipContent = SDL_GetClipboardText();

	if (!ClipContent)
	{
		UE_LOG(LogInit, Fatal, TEXT("Error pasting clipboard contents: %s\n"), UTF8_TO_TCHAR(SDL_GetError()));
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

	FSlowHeartBeatScope SuspendHeartBeat;
	if (SDL_ShowMessageBox(&MessageBoxData, &ButtonPressed) == -1) 
	{
		UE_LOG(LogInit, Fatal, TEXT("Error Presenting MessageBox: %s\n"), UTF8_TO_TCHAR(SDL_GetError()));
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
	static int32 NumberOfCores = 0;
	if (NumberOfCores == 0)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("usehyperthreading")))
		{
			NumberOfCores = NumberOfCoresIncludingHyperthreads();
		}
		else
		{
			cpu_set_t AvailableCpusMask;
			CPU_ZERO(&AvailableCpusMask);

			if (0 != sched_getaffinity(0, sizeof(AvailableCpusMask), &AvailableCpusMask))
			{
				NumberOfCores = 1;	// we are running on something, right?
			}
			else
			{
				char FileNameBuffer[1024];
				struct CpuInfo
				{
					int Core;
					int Package;
				}
				CpuInfos[CPU_SETSIZE];

				FMemory::Memzero(CpuInfos);
				int MaxCoreId = 0;
				int MaxPackageId = 0;

				for(int32 CpuIdx = 0; CpuIdx < CPU_SETSIZE; ++CpuIdx)
				{
					if (CPU_ISSET(CpuIdx, &AvailableCpusMask))
					{
						sprintf(FileNameBuffer, "/sys/devices/system/cpu/cpu%d/topology/core_id", CpuIdx);

						if (FILE* CoreIdFile = fopen(FileNameBuffer, "r"))
						{
							if (1 != fscanf(CoreIdFile, "%d", &CpuInfos[CpuIdx].Core))
							{
								CpuInfos[CpuIdx].Core = 0;
							}
							fclose(CoreIdFile);
						}

						sprintf(FileNameBuffer, "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", CpuIdx);

						unsigned int PackageId = 0;
						if (FILE* PackageIdFile = fopen(FileNameBuffer, "r"))
						{
							if (1 != fscanf(PackageIdFile, "%d", &CpuInfos[CpuIdx].Package))
							{
								CpuInfos[CpuIdx].Package = 0;
							}
							fclose(PackageIdFile);
						}

						MaxCoreId = FMath::Max(MaxCoreId, CpuInfos[CpuIdx].Core);
						MaxPackageId = FMath::Max(MaxPackageId, CpuInfos[CpuIdx].Package);
					}
				}

				int NumCores = MaxCoreId + 1;
				int NumPackages = MaxPackageId + 1;
				int NumPairs = NumPackages * NumCores;
				unsigned char * Pairs = reinterpret_cast<unsigned char *>(FMemory_Alloca(NumPairs * sizeof(unsigned char)));
				FMemory::Memzero(Pairs, NumPairs * sizeof(unsigned char));

				for (int32 CpuIdx = 0; CpuIdx < CPU_SETSIZE; ++CpuIdx)
				{
					if (CPU_ISSET(CpuIdx, &AvailableCpusMask))
					{
						Pairs[CpuInfos[CpuIdx].Package * NumCores + CpuInfos[CpuIdx].Core] = 1;
					}
				}

				for (int32 Idx = 0; Idx < NumPairs; ++Idx)
				{
					NumberOfCores += Pairs[Idx];
				}
			}
		}
	}

	return NumberOfCores;
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

bool FLinuxPlatformMisc::HasCPUIDInstruction()
{
#if PLATFORM_HAS_CPUID
	return __get_cpuid_max(0, 0) != 0;
#else
	return false;	// Linux ARM or something more exotic
#endif // PLATFORM_HAS_CPUID
}

FString FLinuxPlatformMisc::GetCPUVendor()
{
	static TCHAR Result[13] = TEXT("NonX86Vendor");
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
#if PLATFORM_HAS_CPUID
		union
		{
			char Buffer[12 + 1];
			struct
			{
				int dw0;
				int dw1;
				int dw2;
			} Dw;
		} VendorResult;

		int Dummy;
		__cpuid(0, Dummy, VendorResult.Dw.dw0, VendorResult.Dw.dw2, VendorResult.Dw.dw1);

		VendorResult.Buffer[12] = 0;

		FCString::Strncpy(Result, UTF8_TO_TCHAR(VendorResult.Buffer), ARRAY_COUNT(Result));
#else
		// use /proc?
#endif // PLATFORM_HAS_CPUID

		bHaveResult = true;
	}

	return FString(Result);
}

uint32 FLinuxPlatformMisc::GetCPUInfo()
{
	static uint32 Info = 0;
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
#if PLATFORM_HAS_CPUID
		int Dummy[3];
		__cpuid(1, Info, Dummy[0], Dummy[1], Dummy[2]);
#endif // PLATFORM_HAS_CPUID

		bHaveResult = true;
	}

	return Info;
}

FString FLinuxPlatformMisc::GetCPUBrand()
{
	static TCHAR Result[64] = TEXT("NonX86CPUBrand");
	static bool bHaveResult = false;

	if (!bHaveResult)
	{
#if PLATFORM_HAS_CPUID
		// @see for more information http://msdn.microsoft.com/en-us/library/vstudio/hskdteyh(v=vs.100).aspx
		ANSICHAR BrandString[0x40] = { 0 };
		int32 CPUInfo[4] = { -1 };
		const SIZE_T CPUInfoSize = sizeof(CPUInfo);

		__cpuid(0x80000000, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
		const uint32 MaxExtIDs = CPUInfo[0];

		if (MaxExtIDs >= 0x80000004)
		{
			const uint32 FirstBrandString = 0x80000002;
			const uint32 NumBrandStrings = 3;
			for (uint32 Index = 0; Index < NumBrandStrings; ++Index)
			{
				__cpuid(FirstBrandString + Index, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
				FPlatformMemory::Memcpy(BrandString + CPUInfoSize * Index, CPUInfo, CPUInfoSize);
			}
		}

		FCString::Strncpy(Result, UTF8_TO_TCHAR(BrandString), ARRAY_COUNT(Result));
#else
		// use /proc?
#endif // PLATFORM_HAS_CPUID

		bHaveResult = true;
	}

	return FString(Result);
}

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

#if !UE_BUILD_SHIPPING
void FLinuxPlatformMisc::UngrabAllInput()
{
	if (GInitializedSDL)
	{
		SDL_Window * GrabbedWindow = SDL_GetGrabbedWindow();
		if (GrabbedWindow)
		{
			SDL_SetWindowGrab(GrabbedWindow, SDL_FALSE);
			SDL_SetKeyboardGrab(GrabbedWindow, SDL_FALSE);
		}

		SDL_CaptureMouse(SDL_FALSE);
	}
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
				CachedResult = UTF8_TO_TCHAR(Buffer);
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

bool FLinuxPlatformMisc::GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
	struct statfs FSStat = { 0 };
	FTCHARToUTF8 Converter(*InPath);
	int Err = statfs((ANSICHAR*)Converter.Get(), &FSStat);
	if (Err == 0)
	{
		TotalNumberOfBytes = FSStat.f_blocks * FSStat.f_bsize;
		NumberOfFreeBytes = FSStat.f_bavail * FSStat.f_bsize;
	}
	else
	{
		int ErrNo = errno;
		UE_LOG(LogLinux, Warning, TEXT("Unable to statfs('%s'): errno=%d (%s)"), *InPath, ErrNo, UTF8_TO_TCHAR(strerror(ErrNo)));
	}
	return (Err == 0);
}


TArray<uint8> FLinuxPlatformMisc::GetMacAddress()
{
	struct ifaddrs *ifap, *ifaptr;
	TArray<uint8> Result;

	if (getifaddrs(&ifap) == 0)
	{
		for (ifaptr = ifap; ifaptr != nullptr; ifaptr = (ifaptr)->ifa_next)
		{
			struct ifreq ifr;

			strncpy(ifr.ifr_name, ifaptr->ifa_name, IFNAMSIZ-1);

			int Socket = socket(AF_UNIX, SOCK_DGRAM, 0);
			if (Socket == -1)
			{
				continue;
			}

			if (ioctl(Socket, SIOCGIFHWADDR, &ifr) == -1)
			{
				close(Socket);
				continue;
			}

			close(Socket);

			if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
			{
				continue;
			}

			const uint8 *MAC = (uint8 *) ifr.ifr_hwaddr.sa_data;

			for (int32 i=0; i < 6; i++)
			{
				Result.Add(MAC[i]);
			}

			break;
		}

		freeifaddrs(ifap);
	}

	return Result;
}


static int64 LastBatteryCheck = 0;
static bool bIsOnBattery = false;

bool FLinuxPlatformMisc::IsRunningOnBattery()
{
	char Scratch[8];
	FDateTime Time = FDateTime::Now();
	int64 Seconds = Time.ToUnixTimestamp();

	// don't poll the OS for battery state on every tick. Just do it once every 10 seconds.
	if (LastBatteryCheck != 0 && (Seconds - LastBatteryCheck) < 10)
	{
		return bIsOnBattery;
	}

	LastBatteryCheck = Seconds;
	bIsOnBattery = false;

	// [RCL] 2015-09-30 FIXME: find a more robust way?
	const int kHardCodedNumBatteries = 10;
	for (int IdxBattery = 0; IdxBattery < kHardCodedNumBatteries; ++IdxBattery)
	{
		char Filename[128];
		sprintf(Filename, "/sys/class/power_supply/ADP%d/online", IdxBattery);

		int State = open(Filename, O_RDONLY);
		if (State != -1)
		{
			// found ACAD device. check its state.
			ssize_t ReadBytes = read(State, Scratch, 1);
			close(State);

			if (ReadBytes > 0)
			{
				bIsOnBattery = (Scratch[0] == '0');
			}

			break;	// quit checking after we found at least one
		}
	}

	// lack of ADP most likely means that we're not on laptop at all

	return bIsOnBattery;
}
