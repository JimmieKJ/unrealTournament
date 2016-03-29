// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AndroidInputInterface.h"
#include "AndroidApplication.h"
#include <android/log.h>
#include <cpu-features.h>
#include "ModuleManager.h"
#include <android/keycodes.h>
#include <string.h>
#include <dlfcn.h>

#include "AndroidPlatformCrashContext.h"
#include "PlatformMallocCrash.h"
#include "AndroidJavaMessageBox.h"
#include "GenericPlatformChunkInstall.h"

#include <android_native_app_glue.h>

DECLARE_LOG_CATEGORY_EXTERN(LogEngine, Log, All);

void* FAndroidMisc::NativeWindow = NULL;

// run time compatibility information
FString FAndroidMisc::AndroidVersion; // version of android we are running eg "4.0.4"
FString FAndroidMisc::DeviceMake; // make of the device we are running on eg. "samsung"
FString FAndroidMisc::DeviceModel; // model of the device we are running on eg "SAMSUNG-SGH-I437"
FString FAndroidMisc::OSLanguage; // language code the device is set to eg "deu"

// Build/API level we are running.
int32 FAndroidMisc::AndroidBuildVersion = 0;

// Whether or not the system handles the volume buttons (event will still be generated either way)
bool FAndroidMisc::VolumeButtonsHandledBySystem = true;

GenericApplication* FAndroidMisc::CreateApplication()
{
	return FAndroidApplication::CreateAndroidApplication();
}

extern void AndroidThunkCpp_ForceQuit();

void FAndroidMisc::RequestExit( bool Force )
{
	UE_LOG(LogWindows, Log, TEXT("FAndroidMisc::RequestExit(%i)"), Force);
	if (Force)
	{
		AndroidThunkCpp_ForceQuit();
	}
	else
	{
		GIsRequestingExit = 1;
	}
}

extern void AndroidThunkCpp_Minimize();

void FAndroidMisc::RequestMinimize()
{
	AndroidThunkCpp_Minimize();
}


void FAndroidMisc::LowLevelOutputDebugString(const TCHAR *Message)
{
	LocalPrint(Message);
}

void FAndroidMisc::LocalPrint(const TCHAR *Message)
{
	// Builds for distribution should not have logging in them:
	// http://developer.android.com/tools/publishing/preparing.html#publishing-configure
#if !UE_BUILD_SHIPPING
	const int MAX_LOG_LENGTH = 4096;
	// not static since may be called by different threads
	ANSICHAR MessageBuffer[MAX_LOG_LENGTH];

	const TCHAR* SourcePtr = Message;
	while (*SourcePtr)
	{
		ANSICHAR* WritePtr = MessageBuffer;
		int32 RemainingSpace = MAX_LOG_LENGTH;
		while (*SourcePtr && --RemainingSpace > 0)
		{
			if (*SourcePtr == TEXT('\r'))
			{
				// If next character is newline, skip it
				if (*(++SourcePtr) == TEXT('\n'))
					++SourcePtr;
				break;
			}
			else if (*SourcePtr == TEXT('\n'))
			{
				++SourcePtr;
				break;
			}
			else {
				*WritePtr++ = static_cast<ANSICHAR>(*SourcePtr++);
			}
		}
		*WritePtr = '\0';
		__android_log_write(ANDROID_LOG_DEBUG, "UE4", MessageBuffer);
	}
	//	__android_log_print(ANDROID_LOG_DEBUG, "UE4", "%s", TCHAR_TO_ANSI(Message));
#endif
}

void FAndroidMisc::LoadPreInitModules()
{
	FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
	FModuleManager::Get().LoadModule(TEXT("AndroidAudio"));
}

void FAndroidMisc::PlatformPreInit()
{
	FGenericPlatformMisc::PlatformPreInit();
	FAndroidAppEntry::PlatformInit();
}

static volatile bool HeadPhonesArePluggedIn = false;

static FAndroidMisc::FBatteryState CurrentBatteryState;

static FCriticalSection ReceiversLock;
static struct  
{
	int		Volume;
	double	TimeOfChange;
} CurrentVolume;

extern "C"
{

	JNIEXPORT void Java_com_epicgames_ue4_HeadsetReceiver_stateChanged(JNIEnv * jni, jclass clazz, jint state)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("nativeHeadsetEvent(%i)"), state);
		HeadPhonesArePluggedIn = (state == 1);
	}

	JNIEXPORT void Java_com_epicgames_ue4_VolumeReceiver_volumeChanged(JNIEnv * jni, jclass clazz, jint volume)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("nativeVolumeEvent(%i)"), volume);
		ReceiversLock.Lock();
		CurrentVolume.Volume = volume;
		CurrentVolume.TimeOfChange = FApp::GetCurrentTime();
		ReceiversLock.Unlock();
	}

	JNIEXPORT void Java_com_epicgames_ue4_BatteryReceiver_dispatchEvent(JNIEnv * jni, jclass clazz, jint status, jint level, jint temperature)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("nativeBatteryEvent(stat = %i, lvl = %i, t = %3.2f)"), status, level, float(temperature)/10.f);

		ReceiversLock.Lock();
		FAndroidMisc::FBatteryState state;
		state.State = (FAndroidMisc::EBatteryState)status;
		state.Level = level;
		state.Temperature = float(temperature)/10.f;
		CurrentBatteryState = state;
		ReceiversLock.Unlock();
	}
}

void FAndroidMisc::PlatformInit()
{
	// Increase the maximum number of simultaneously open files
	// Display Timer resolution.
	// Get swap file info
	// Display memory info

	// Register natives to receive Volume, Battery, Headphones events
	JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
	if (nullptr != JEnv)
	{
		struct
		{
			const char*		ClazzName;
			JNINativeMethod	Jnim;
			jclass			Clazz;
		} gMethods[] =
		{
			{ "com/epicgames/ue4/VolumeReceiver",  { "volumeChanged", "(I)V",  (void *)Java_com_epicgames_ue4_VolumeReceiver_volumeChanged } },
			{ "com/epicgames/ue4/BatteryReceiver", { "dispatchEvent", "(III)V",(void *)Java_com_epicgames_ue4_BatteryReceiver_dispatchEvent } },
			{ "com/epicgames/ue4/HeadsetReceiver", { "stateChanged",  "(I)V",  (void *)Java_com_epicgames_ue4_HeadsetReceiver_stateChanged } },
		};
		const int count = sizeof(gMethods) / sizeof(gMethods[0]);

		for (int i = 0; i < count; i++)
		{
			gMethods[i].Clazz = FAndroidApplication::FindJavaClass(gMethods[i].ClazzName);
			if (gMethods[i].Clazz == nullptr)
			{
				UE_LOG(LogEngine, Warning, TEXT("Can't find class for %s"), gMethods[i].ClazzName);
				continue;
			}
			if (JNI_OK != JEnv->RegisterNatives(gMethods[i].Clazz, &gMethods[i].Jnim, 1))
			{
				UE_LOG(LogEngine, Warning, TEXT("RegisterNatives failed for %s on %s"), gMethods[i].ClazzName, gMethods[i].Jnim.name);
			}
			extern struct android_app* GNativeAndroidApp;
			jmethodID methodId = JEnv->GetStaticMethodID(gMethods[i].Clazz, "startReceiver", "(Landroid/app/Activity;)V");
			if (methodId != 0)
			{
				JEnv->CallStaticVoidMethod(gMethods[i].Clazz, methodId, GNativeAndroidApp->activity->clazz);
			}
			else
			{
				UE_LOG(LogEngine, Warning, TEXT("Can't find method startReceiver of class %s"), gMethods[i].ClazzName);
			}
		}
	}
}

extern void AndroidThunkCpp_DismissSplashScreen();

void FAndroidMisc::PlatformPostInit(bool ShowSplashScreen)
{
	if (!ShowSplashScreen)
	{
		AndroidThunkCpp_DismissSplashScreen();
	}
}

void FAndroidMisc::GetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, int32 ResultLength)
{
	*Result = 0;
	// @todo Android : get environment variable.
}

void FAndroidMisc::SetHardwareWindow(void* InWindow)
{
	NativeWindow = InWindow; //using raw native window handle for now. Could be changed to use AndroidWindow later if needed
}


void* FAndroidMisc::GetHardwareWindow()
{
	return NativeWindow;
}

const TCHAR* FAndroidMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	if (Error == 0)
	{
		Error = errno;
	}
	char ErrorBuffer[1024];
	strerror_r(Error, ErrorBuffer, 1024);
	FCString::Strcpy(OutBuffer, BufferCount, UTF8_TO_TCHAR((const ANSICHAR*)ErrorBuffer));
	return OutBuffer;
}

void FAndroidMisc::ClipboardCopy(const TCHAR* Str)
{
	//@todo Android
}

void FAndroidMisc::ClipboardPaste(class FString& Result)
{
	Result = TEXT("");
	//@todo Android
}

EAppReturnType::Type FAndroidMisc::MessageBoxExt( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption )
{
	FJavaAndroidMessageBox MessageBox;
	MessageBox.SetText(Text);
	MessageBox.SetCaption(Caption);
	EAppReturnType::Type * ResultValues = nullptr;
	static EAppReturnType::Type ResultsOk[] = {
		EAppReturnType::Ok };
	static EAppReturnType::Type ResultsYesNo[] = {
		EAppReturnType::Yes, EAppReturnType::No };
	static EAppReturnType::Type ResultsOkCancel[] = {
		EAppReturnType::Ok, EAppReturnType::Cancel };
	static EAppReturnType::Type ResultsYesNoCancel[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::Cancel };
	static EAppReturnType::Type ResultsCancelRetryContinue[] = {
		EAppReturnType::Cancel, EAppReturnType::Retry, EAppReturnType::Continue };
	static EAppReturnType::Type ResultsYesNoYesAllNoAll[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::YesAll,
		EAppReturnType::NoAll };
	static EAppReturnType::Type ResultsYesNoYesAllNoAllCancel[] = {
		EAppReturnType::Yes, EAppReturnType::No, EAppReturnType::YesAll,
		EAppReturnType::NoAll, EAppReturnType::Cancel };

	// TODO: Should we localize button text?

	switch (MsgType)
	{
	case EAppMsgType::Ok:
		MessageBox.AddButton(TEXT("Ok"));
		ResultValues = ResultsOk;
		break;
	case EAppMsgType::YesNo:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		ResultValues = ResultsYesNo;
		break;
	case EAppMsgType::OkCancel:
		MessageBox.AddButton(TEXT("Ok"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsOkCancel;
		break;
	case EAppMsgType::YesNoCancel:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsYesNoCancel;
		break;
	case EAppMsgType::CancelRetryContinue:
		MessageBox.AddButton(TEXT("Cancel"));
		MessageBox.AddButton(TEXT("Retry"));
		MessageBox.AddButton(TEXT("Continue"));
		ResultValues = ResultsCancelRetryContinue;
		break;
	case EAppMsgType::YesNoYesAllNoAll:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Yes To All"));
		MessageBox.AddButton(TEXT("No To All"));
		ResultValues = ResultsYesNoYesAllNoAll;
		break;
	case EAppMsgType::YesNoYesAllNoAllCancel:
		MessageBox.AddButton(TEXT("Yes"));
		MessageBox.AddButton(TEXT("No"));
		MessageBox.AddButton(TEXT("Yes To All"));
		MessageBox.AddButton(TEXT("No To All"));
		MessageBox.AddButton(TEXT("Cancel"));
		ResultValues = ResultsYesNoYesAllNoAllCancel;
		break;
	default:
		check(0);
	}
	int32 Choice = MessageBox.Show();
	if (Choice >= 0 && nullptr != ResultValues)
	{
		return ResultValues[Choice];
	}
	// Failed to show dialog, or failed to get a response,
	// return default cancel response instead.
	return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
}

extern void AndroidThunkCpp_KeepScreenOn(bool Enable);

bool FAndroidMisc::ControlScreensaver(EScreenSaverAction Action)
{
	switch (Action)
	{
		case EScreenSaverAction::Disable:
			// Prevent display sleep.
			AndroidThunkCpp_KeepScreenOn(true);
			break;

		case EScreenSaverAction::Enable:
			// Stop preventing display sleep now that we are done.
			AndroidThunkCpp_KeepScreenOn(false);
			break;
	}
	return true;
}

bool FAndroidMisc::HasPlatformFeature(const TCHAR* FeatureName)
{
	if (FCString::Stricmp(FeatureName, TEXT("Vulkan")) == 0)
	{
		return FAndroidMisc::ShouldUseVulkan();
	}

	return FGenericPlatformMisc::HasPlatformFeature(FeatureName);
}

bool FAndroidMisc::AllowRenderThread()
{
	if (FAndroidMisc::ShouldUseVulkan())
	{
		// @todo vulkan: stop forcing no RT!
		return false;
	}

	// there is a crash with the nvidia tegra dual core processors namely the optimus 2x and xoom 
	// when running multithreaded it can't handle multiple threads using opengl (bug)
	// tested with lg optimus 2x and motorola xoom 
	// come back and revisit this later 
	// https://code.google.com/p/android/issues/detail?id=32636
	if (FAndroidMisc::GetGPUFamily() == FString(TEXT("NVIDIA Tegra")) && FPlatformMisc::NumberOfCores() <= 2 && FAndroidMisc::GetGLVersion().StartsWith(TEXT("OpenGL ES 2.")))
	{
		return false;
	}

	// Vivante GC1000 with 2.x driver has issues with render thread
	if (FAndroidMisc::GetGPUFamily().StartsWith(TEXT("Vivante GC1000")) && FAndroidMisc::GetGLVersion().StartsWith(TEXT("OpenGL ES 2.")))
	{
		return false;
	}

	// there is an issue with presenting the buffer on kindle fire (1st gen) with multiple threads using opengl
	if (FAndroidMisc::GetDeviceModel() == FString(TEXT("Kindle Fire")))
	{
		return false;
	}

	// there is an issue with swapbuffer ordering on startup on samsung s3 mini with multiple threads using opengl
	if (FAndroidMisc::GetDeviceModel() == FString(TEXT("GT-I8190L")))
	{
		return false;
	}

	return true;
}

int32 FAndroidMisc::NumberOfCores()
{
	int32 NumberOfCores = android_getCpuCount();
	return NumberOfCores;
}

static FAndroidMisc::FCPUState CurrentCPUState;

FAndroidMisc::FCPUState& FAndroidMisc::GetCPUState(){
	uint64_t UserTime, NiceTime, SystemTime, SoftIRQTime, IRQTime, IdleTime, IOWaitTime;
	int32		Index = 0;
	ANSICHAR	Buffer[500];

	CurrentCPUState.CoreCount = FAndroidMisc::NumberOfCores();
	FILE* FileHandle = fopen("/proc/stat", "r");
	if (FileHandle){
		CurrentCPUState.ActivatedCoreCount = 0;
		for (size_t n = 0; n < CurrentCPUState.CoreCount; n++) {
			CurrentCPUState.Status[n] = 0;
			CurrentCPUState.PreviousUsage[n] = CurrentCPUState.CurrentUsage[n];
		}
		
		while (fgets(Buffer, 100, FileHandle)) {
			sscanf(Buffer, "%4s %8llu %8llu %8llu %8llu %8llu %8llu %8llu", CurrentCPUState.Name,
				&UserTime, &NiceTime, &SystemTime, &IdleTime, &IOWaitTime, &IRQTime,
				&SoftIRQTime);

			if (0 == strncmp(CurrentCPUState.Name, "cpu", 3)) {
				Index = CurrentCPUState.Name[3] - '0';
				if (Index >= 0 && Index < CurrentCPUState.CoreCount) {
					CurrentCPUState.CurrentUsage[Index].IdleTime = IdleTime;
					CurrentCPUState.CurrentUsage[Index].NiceTime = NiceTime;
					CurrentCPUState.CurrentUsage[Index].SystemTime = SystemTime;
					CurrentCPUState.CurrentUsage[Index].SoftIRQTime = SoftIRQTime;
					CurrentCPUState.CurrentUsage[Index].IRQTime = IRQTime;
					CurrentCPUState.CurrentUsage[Index].IOWaitTime = IOWaitTime;
					CurrentCPUState.CurrentUsage[Index].UserTime = UserTime;
					CurrentCPUState.CurrentUsage[Index].TotalTime = UserTime + NiceTime + SystemTime + SoftIRQTime + IRQTime + IdleTime + IOWaitTime;
					CurrentCPUState.Status[Index] = 1;
					CurrentCPUState.ActivatedCoreCount++;
				}
				if (Index == CurrentCPUState.CoreCount-1)
					break;
			}
		}
		fclose(FileHandle);

		double WallTime;
		double CPULoad[CurrentCPUState.CoreCount];
		CurrentCPUState.AverageUtilization = 0.0; 
		for (size_t n = 0; n < CurrentCPUState.CoreCount; n++) {
			if (CurrentCPUState.CurrentUsage[n].TotalTime <= CurrentCPUState.PreviousUsage[n].TotalTime) {
				CPULoad[n] = 0;
				continue;
			}

			WallTime = CurrentCPUState.CurrentUsage[n].TotalTime - CurrentCPUState.PreviousUsage[n].TotalTime;
			IdleTime = CurrentCPUState.CurrentUsage[n].IdleTime - CurrentCPUState.PreviousUsage[n].IdleTime;

			if (!WallTime || WallTime <= IdleTime) {
				CPULoad[n] = 0;
				continue;
			}
			CPULoad[n] = (WallTime - (double)IdleTime) * 100.0 / WallTime;
			CurrentCPUState.Utilization[n] = CPULoad[n];
			CurrentCPUState.AverageUtilization += CPULoad[n];
		}
		CurrentCPUState.AverageUtilization /= (double)CurrentCPUState.CoreCount;
	}else{
		FMemory::Memzero(CurrentCPUState);		
	}
	return CurrentCPUState;
}


extern FString GFilePathBase;
extern FString GFontPathBase;

class FTestUtime
{
public:
	FTestUtime()
		: Supported(false)
	{
		static FString TestFilePath = GFilePathBase + FString(TEXT("/UE4UtimeTest.txt"));
		static const char * TestFilePathChar = StringCast<ANSICHAR>(*TestFilePath).Get();
		FILE * FileHandle = fopen(TestFilePathChar, "w");
		if(FileHandle)
		{
			fclose(FileHandle);

			// get file times
			struct stat FileInfo;
			if (stat(TestFilePathChar, &FileInfo) == -1)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Unable to get file stamp for file ('%s')"), TestFilePathChar);
			}
			else
			{
				struct utimbuf Times;
			    Times.actime = 0;
			    Times.modtime = 0;
				int Result = utime(TestFilePathChar, &Times);
				Supported = -1 != Result;
				unlink(TestFilePathChar);

				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("UTime failed for local caching supported test, with error code %d\n"), Result);
			}
			
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to create file for Local cache file test\n"), Supported);
		}
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Is Local Caching Supported? %d\n"), Supported);
	}

	bool Supported;
};

bool SupportsUTime()
{
	static FTestUtime Test;
	return Test.Supported;
}


bool FAndroidMisc::SupportsLocalCaching()
{
	return true;

	/*if ( SupportsUTime() )
	{
		return true;
	}*/


}

/**
 * Good enough default crash reporter.
 */
void DefaultCrashHandler(const FAndroidCrashContext& Context)
{
	static int32 bHasEntered = 0;
	if (FPlatformAtomics::InterlockedCompareExchange(&bHasEntered, 1, 0) == 0)
	{
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR StackTrace[StackTraceSize];
		StackTrace[0] = 0;

		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, Context.Context);
		UE_LOG(LogEngine, Error, TEXT("\n%s\n"), ANSI_TO_TCHAR(StackTrace));

		if (GLog)
		{
			GLog->SetCurrentThreadAsMasterThread();
			GLog->Flush();
		}
		
		if (GWarn)
		{
			GWarn->Flush();
		}
	}
}

/** Global pointer to crash handler */
void (* GCrashHandlerPointer)(const FGenericCrashContext& Context) = NULL;

const int32 TargetSignals[] = 
{
	SIGQUIT, // SIGQUIT is a user-initiated "crash".
	SIGILL,
	SIGFPE,
	SIGBUS,
	SIGSEGV,
	SIGSYS
};

const int32 NumTargetSignals = ARRAY_COUNT(TargetSignals);

struct sigaction PrevActions[NumTargetSignals];

static void RestorePreviousSignalHandlers()
{
	for (int32 i = 0; i < NumTargetSignals; ++i)
	{
		sigaction(TargetSignals[i],	&PrevActions[i], NULL);
	}
}

/** True system-specific crash handler that gets called first */
void PlatformCrashHandler(int32 Signal, siginfo* Info, void* Context)
{
	// Switch to malloc crash.
	//FGenericPlatformMallocCrash::Get().SetAsGMalloc(); @todo uncomment after verification

	//fprintf(stderr, "Signal %d caught.\n", Signal);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Signal %d caught!"), Signal);

	// Restore system handlers so Android could catch this signal after we are done with crashreport
	RestorePreviousSignalHandlers();

	FAndroidCrashContext CrashContext;
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

void FAndroidMisc::SetCrashHandler(void (* CrashHandler)(const FGenericCrashContext& Context))
{
	GCrashHandlerPointer = CrashHandler;

	FMemory::Memzero(&PrevActions, sizeof(PrevActions));

	struct sigaction Action;
	FMemory::Memzero(&Action, sizeof(struct sigaction));
	Action.sa_sigaction = PlatformCrashHandler;
	sigemptyset(&Action.sa_mask);
	Action.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

	for (int32 i = 0; i < NumTargetSignals; ++i)
	{
		sigaction(TargetSignals[i],	&Action, &PrevActions[i]);
	}
}

bool FAndroidMisc::GetUseVirtualJoysticks()
{
	return !FParse::Param(FCommandLine::Get(),TEXT("joystick"));
}

TArray<uint8> FAndroidMisc::GetSystemFontBytes()
{
	TArray<uint8> FontBytes;
	static FString FullFontPath = GFontPathBase + FString(TEXT("DroidSans.ttf"));
	FFileHelper::LoadFileToArray(FontBytes, *FullFontPath);
	return FontBytes;
}

class IPlatformChunkInstall* FAndroidMisc::GetPlatformChunkInstall()
{
	static IPlatformChunkInstall* ChunkInstall = nullptr;
	IPlatformChunkInstallModule* PlatformChunkInstallModule = FModuleManager::LoadModulePtr<IPlatformChunkInstallModule>("HTTPChunkInstaller");
	if (!ChunkInstall)
	{
		if (PlatformChunkInstallModule != NULL)
		{
			// Attempt to grab the platform installer
			ChunkInstall = PlatformChunkInstallModule->GetPlatformChunkInstall();
		}
		else
		{
			// Placeholder instance
			ChunkInstall = new FGenericPlatformChunkInstall();
		}
	}

	return ChunkInstall;
}

void FAndroidMisc::SetVersionInfo( FString InAndroidVersion, FString InDeviceMake, FString InDeviceModel, FString InOSLanguage )
{
	AndroidVersion = InAndroidVersion;
	DeviceMake = InDeviceMake;
	DeviceModel = InDeviceModel;
	OSLanguage = InOSLanguage;

	UE_LOG(LogEngine, Display, TEXT("Android Version Make Model Language: %s %s %s %s"), *AndroidVersion, *DeviceMake, *DeviceModel, *OSLanguage);
}

const FString FAndroidMisc::GetAndroidVersion()
{
	return AndroidVersion;
}

const FString FAndroidMisc::GetDeviceMake()
{
	return DeviceMake;
}

const FString FAndroidMisc::GetDeviceModel()
{
	return DeviceModel;
}

const FString FAndroidMisc::GetOSLanguage()
{
	return OSLanguage;
}

FString FAndroidMisc::GetDefaultLocale()
{
	return OSLanguage;
}

uint32 FAndroidMisc::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; if(KeyNames) { KeyNames[NumMappings]=KeyName; } ++NumMappings; };

	uint32 NumMappings = 0;

	if ( KeyCodes && (MaxMappings > 0) )
	{
		ADDKEYMAP( AKEYCODE_0, TEXT("Zero") );
		ADDKEYMAP( AKEYCODE_1, TEXT("One") );
		ADDKEYMAP( AKEYCODE_2, TEXT("Two") );
		ADDKEYMAP( AKEYCODE_3, TEXT("Three") );
		ADDKEYMAP( AKEYCODE_4, TEXT("Four") );
		ADDKEYMAP( AKEYCODE_5, TEXT("Five") );
		ADDKEYMAP( AKEYCODE_6, TEXT("Six") );
		ADDKEYMAP( AKEYCODE_7, TEXT("Seven") );
		ADDKEYMAP( AKEYCODE_8, TEXT("Eight") );
		ADDKEYMAP( AKEYCODE_9, TEXT("Nine") );

		ADDKEYMAP( AKEYCODE_A, TEXT("A") );
		ADDKEYMAP( AKEYCODE_B, TEXT("B") );
		ADDKEYMAP( AKEYCODE_C, TEXT("C") );
		ADDKEYMAP( AKEYCODE_D, TEXT("D") );
		ADDKEYMAP( AKEYCODE_E, TEXT("E") );
		ADDKEYMAP( AKEYCODE_F, TEXT("F") );
		ADDKEYMAP( AKEYCODE_G, TEXT("G") );
		ADDKEYMAP( AKEYCODE_H, TEXT("H") );
		ADDKEYMAP( AKEYCODE_I, TEXT("I") );
		ADDKEYMAP( AKEYCODE_J, TEXT("J") );
		ADDKEYMAP( AKEYCODE_K, TEXT("K") );
		ADDKEYMAP( AKEYCODE_L, TEXT("L") );
		ADDKEYMAP( AKEYCODE_M, TEXT("M") );
		ADDKEYMAP( AKEYCODE_N, TEXT("N") );
		ADDKEYMAP( AKEYCODE_O, TEXT("O") );
		ADDKEYMAP( AKEYCODE_P, TEXT("P") );
		ADDKEYMAP( AKEYCODE_Q, TEXT("Q") );
		ADDKEYMAP( AKEYCODE_R, TEXT("R") );
		ADDKEYMAP( AKEYCODE_S, TEXT("S") );
		ADDKEYMAP( AKEYCODE_T, TEXT("T") );
		ADDKEYMAP( AKEYCODE_U, TEXT("U") );
		ADDKEYMAP( AKEYCODE_V, TEXT("V") );
		ADDKEYMAP( AKEYCODE_W, TEXT("W") );
		ADDKEYMAP( AKEYCODE_X, TEXT("X") );
		ADDKEYMAP( AKEYCODE_Y, TEXT("Y") );
		ADDKEYMAP( AKEYCODE_Z, TEXT("Z") );

		ADDKEYMAP( AKEYCODE_SEMICOLON, TEXT("Semicolon") );
		ADDKEYMAP( AKEYCODE_EQUALS, TEXT("Equals") );
		ADDKEYMAP( AKEYCODE_COMMA, TEXT("Comma") );
		//ADDKEYMAP( '-', TEXT("Underscore") );
		ADDKEYMAP( AKEYCODE_PERIOD, TEXT("Period") );
		ADDKEYMAP( AKEYCODE_SLASH, TEXT("Slash") );
		ADDKEYMAP( AKEYCODE_GRAVE, TEXT("Tilde") );
		ADDKEYMAP( AKEYCODE_LEFT_BRACKET, TEXT("LeftBracket") );
		ADDKEYMAP( AKEYCODE_BACKSLASH, TEXT("Backslash") );
		ADDKEYMAP( AKEYCODE_RIGHT_BRACKET, TEXT("RightBracket") );
		ADDKEYMAP( AKEYCODE_APOSTROPHE, TEXT("Quote") );
		ADDKEYMAP( AKEYCODE_SPACE, TEXT("SpaceBar") );

		//ADDKEYMAP( VK_LBUTTON, TEXT("LeftMouseButton") );
		//ADDKEYMAP( VK_RBUTTON, TEXT("RightMouseButton") );
		//ADDKEYMAP( VK_MBUTTON, TEXT("MiddleMouseButton") );

		//ADDKEYMAP( AKEYCODE_BUTTON_THUMBL, TEXT("ThumbMouseButton") );
		//ADDKEYMAP( AKEYCODE_BUTTON_THUMBR, TEXT("ThumbMouseButton2") );

		ADDKEYMAP( AKEYCODE_DEL, TEXT("BackSpace") );
		ADDKEYMAP( AKEYCODE_TAB, TEXT("Tab") );
		ADDKEYMAP( AKEYCODE_ENTER, TEXT("Enter") );
		ADDKEYMAP( AKEYCODE_BREAK, TEXT("Pause") );

		//ADDKEYMAP( VK_CAPITAL, TEXT("CapsLock") );
		ADDKEYMAP( AKEYCODE_BACK, TEXT("Escape") );
		ADDKEYMAP( AKEYCODE_SPACE, TEXT("SpaceBar") );
		ADDKEYMAP( AKEYCODE_PAGE_UP, TEXT("PageUp") );
		ADDKEYMAP( AKEYCODE_PAGE_DOWN, TEXT("PageDown") );
		//ADDKEYMAP( VK_END, TEXT("End") );
		//ADDKEYMAP( VK_HOME, TEXT("Home") );

		//ADDKEYMAP( AKEYCODE_DPAD_LEFT, TEXT("Left") );
		//ADDKEYMAP( AKEYCODE_DPAD_UP, TEXT("Up") );
		//ADDKEYMAP( AKEYCODE_DPAD_RIGHT, TEXT("Right") );
		//ADDKEYMAP( AKEYCODE_DPAD_DOWN, TEXT("Down") );

		//ADDKEYMAP( VK_INSERT, TEXT("Insert") );
		ADDKEYMAP( AKEYCODE_FORWARD_DEL, TEXT("Delete") );

		ADDKEYMAP( AKEYCODE_STAR, TEXT("Multiply") );
		ADDKEYMAP( AKEYCODE_PLUS, TEXT("Add") );
		ADDKEYMAP( AKEYCODE_MINUS, TEXT("Subtract") );
		ADDKEYMAP( AKEYCODE_COMMA, TEXT("Decimal") );
		//ADDKEYMAP( AKEYCODE_SLASH, TEXT("Divide") );

		ADDKEYMAP( AKEYCODE_F1, TEXT("F1") );
		ADDKEYMAP( AKEYCODE_F2, TEXT("F2") );
		ADDKEYMAP( AKEYCODE_F3, TEXT("F3") );
		ADDKEYMAP( AKEYCODE_F4, TEXT("F4") );
		ADDKEYMAP( AKEYCODE_F5, TEXT("F5") );
		ADDKEYMAP( AKEYCODE_F6, TEXT("F6") );
		ADDKEYMAP( AKEYCODE_F7, TEXT("F7") );
		ADDKEYMAP( AKEYCODE_F8, TEXT("F8") );
		ADDKEYMAP( AKEYCODE_F9, TEXT("F9") );
		ADDKEYMAP( AKEYCODE_F10, TEXT("F10") );
		ADDKEYMAP( AKEYCODE_F11, TEXT("F11") );
		ADDKEYMAP( AKEYCODE_F12, TEXT("F12") );

		//ADDKEYMAP( AKEYCODE_NUM, TEXT("NumLock") );

		//ADDKEYMAP( VK_SCROLL, TEXT("ScrollLock") );

		ADDKEYMAP( AKEYCODE_SHIFT_LEFT, TEXT("LeftShift") );
		ADDKEYMAP( AKEYCODE_SHIFT_RIGHT, TEXT("RightShift") );
		ADDKEYMAP( AKEYCODE_CTRL_LEFT, TEXT("LeftControl") );
		ADDKEYMAP( AKEYCODE_CTRL_RIGHT, TEXT("RightControl") );
		ADDKEYMAP( AKEYCODE_ALT_LEFT, TEXT("LeftAlt") );
		ADDKEYMAP( AKEYCODE_ALT_RIGHT, TEXT("RightAlt") );
		ADDKEYMAP( AKEYCODE_META_LEFT, TEXT("LeftCommand") );
		ADDKEYMAP( AKEYCODE_META_RIGHT, TEXT("RightCommand") );
	}

	check(NumMappings < MaxMappings);
	return NumMappings;
#undef ADDKEYMAP

}

uint32 FAndroidMisc::GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; if(KeyNames) { KeyNames[NumMappings]=KeyName; } ++NumMappings; };

	uint32 NumMappings = 0;

	if ( KeyCodes && (MaxMappings > 0) )
	{
		ADDKEYMAP( AKEYCODE_0, TEXT("Zero") );
		ADDKEYMAP( AKEYCODE_1, TEXT("One") );
		ADDKEYMAP( AKEYCODE_2, TEXT("Two") );
		ADDKEYMAP( AKEYCODE_3, TEXT("Three") );
		ADDKEYMAP( AKEYCODE_4, TEXT("Four") );
		ADDKEYMAP( AKEYCODE_5, TEXT("Five") );
		ADDKEYMAP( AKEYCODE_6, TEXT("Six") );
		ADDKEYMAP( AKEYCODE_7, TEXT("Seven") );
		ADDKEYMAP( AKEYCODE_8, TEXT("Eight") );
		ADDKEYMAP( AKEYCODE_9, TEXT("Nine") );

		ADDKEYMAP( AKEYCODE_A, TEXT("A") );
		ADDKEYMAP( AKEYCODE_B, TEXT("B") );
		ADDKEYMAP( AKEYCODE_C, TEXT("C") );
		ADDKEYMAP( AKEYCODE_D, TEXT("D") );
		ADDKEYMAP( AKEYCODE_E, TEXT("E") );
		ADDKEYMAP( AKEYCODE_F, TEXT("F") );
		ADDKEYMAP( AKEYCODE_G, TEXT("G") );
		ADDKEYMAP( AKEYCODE_H, TEXT("H") );
		ADDKEYMAP( AKEYCODE_I, TEXT("I") );
		ADDKEYMAP( AKEYCODE_J, TEXT("J") );
		ADDKEYMAP( AKEYCODE_K, TEXT("K") );
		ADDKEYMAP( AKEYCODE_L, TEXT("L") );
		ADDKEYMAP( AKEYCODE_M, TEXT("M") );
		ADDKEYMAP( AKEYCODE_N, TEXT("N") );
		ADDKEYMAP( AKEYCODE_O, TEXT("O") );
		ADDKEYMAP( AKEYCODE_P, TEXT("P") );
		ADDKEYMAP( AKEYCODE_Q, TEXT("Q") );
		ADDKEYMAP( AKEYCODE_R, TEXT("R") );
		ADDKEYMAP( AKEYCODE_S, TEXT("S") );
		ADDKEYMAP( AKEYCODE_T, TEXT("T") );
		ADDKEYMAP( AKEYCODE_U, TEXT("U") );
		ADDKEYMAP( AKEYCODE_V, TEXT("V") );
		ADDKEYMAP( AKEYCODE_W, TEXT("W") );
		ADDKEYMAP( AKEYCODE_X, TEXT("X") );
		ADDKEYMAP( AKEYCODE_Y, TEXT("Y") );
		ADDKEYMAP( AKEYCODE_Z, TEXT("Z") );

		ADDKEYMAP( AKEYCODE_SEMICOLON, TEXT("Semicolon") );
		ADDKEYMAP( AKEYCODE_EQUALS, TEXT("Equals") );
		ADDKEYMAP( AKEYCODE_COMMA, TEXT("Comma") );
		//ADDKEYMAP( '-', TEXT("Underscore") );
		ADDKEYMAP( AKEYCODE_PERIOD, TEXT("Period") );
		ADDKEYMAP( AKEYCODE_SLASH, TEXT("Slash") );
		ADDKEYMAP( AKEYCODE_GRAVE, TEXT("Tilde") );
		ADDKEYMAP( AKEYCODE_LEFT_BRACKET, TEXT("LeftBracket") );
		ADDKEYMAP( AKEYCODE_BACKSLASH, TEXT("Backslash") );
		ADDKEYMAP( AKEYCODE_RIGHT_BRACKET, TEXT("RightBracket") );
		ADDKEYMAP( AKEYCODE_APOSTROPHE, TEXT("Quote") );
		ADDKEYMAP( AKEYCODE_SPACE, TEXT("SpaceBar") );

		//ADDKEYMAP( VK_LBUTTON, TEXT("LeftMouseButton") );
		//ADDKEYMAP( VK_RBUTTON, TEXT("RightMouseButton") );
		//ADDKEYMAP( VK_MBUTTON, TEXT("MiddleMouseButton") );

		ADDKEYMAP( AKEYCODE_BUTTON_THUMBL, TEXT("ThumbMouseButton") );
		ADDKEYMAP( AKEYCODE_BUTTON_THUMBR, TEXT("ThumbMouseButton2") );

		ADDKEYMAP( AKEYCODE_DEL, TEXT("BackSpace") );
		ADDKEYMAP( AKEYCODE_TAB, TEXT("Tab") );
		ADDKEYMAP( AKEYCODE_ENTER, TEXT("Enter") );
		//ADDKEYMAP( VK_PAUSE, TEXT("Pause") );

		//ADDKEYMAP( VK_CAPITAL, TEXT("CapsLock") );
		ADDKEYMAP( AKEYCODE_BACK, TEXT("Escape") );
		ADDKEYMAP( AKEYCODE_SPACE, TEXT("SpaceBar") );
		ADDKEYMAP( AKEYCODE_PAGE_UP, TEXT("PageUp") );
		ADDKEYMAP( AKEYCODE_PAGE_DOWN, TEXT("PageDown") );
		//ADDKEYMAP( VK_END, TEXT("End") );
		//ADDKEYMAP( VK_HOME, TEXT("Home") );

		ADDKEYMAP( AKEYCODE_DPAD_LEFT, TEXT("Left") );
		ADDKEYMAP( AKEYCODE_DPAD_UP, TEXT("Up") );
		ADDKEYMAP( AKEYCODE_DPAD_RIGHT, TEXT("Right") );
		ADDKEYMAP( AKEYCODE_DPAD_DOWN, TEXT("Down") );

		//ADDKEYMAP( VK_INSERT, TEXT("Insert") );
		//ADDKEYMAP( AKEYCODE_DEL, TEXT("Delete") );

		ADDKEYMAP( AKEYCODE_STAR, TEXT("Multiply") );
		ADDKEYMAP( AKEYCODE_PLUS, TEXT("Add") );
		ADDKEYMAP( AKEYCODE_MINUS, TEXT("Subtract") );
		ADDKEYMAP( AKEYCODE_COMMA, TEXT("Decimal") );
		//ADDKEYMAP( AKEYCODE_SLASH, TEXT("Divide") );

		//ADDKEYMAP( VK_F1, TEXT("F1") );
		//ADDKEYMAP( VK_F2, TEXT("F2") );
		//ADDKEYMAP( VK_F3, TEXT("F3") );
		//ADDKEYMAP( VK_F4, TEXT("F4") );
		//ADDKEYMAP( VK_F5, TEXT("F5") );
		//ADDKEYMAP( VK_F6, TEXT("F6") );
		//ADDKEYMAP( VK_F7, TEXT("F7") );
		//ADDKEYMAP( VK_F8, TEXT("F8") );
		//ADDKEYMAP( VK_F9, TEXT("F9") );
		//ADDKEYMAP( VK_F10, TEXT("F10") );
		//ADDKEYMAP( VK_F11, TEXT("F11") );
		//ADDKEYMAP( VK_F12, TEXT("F12") );

		ADDKEYMAP( AKEYCODE_NUM, TEXT("NumLock") );

		//ADDKEYMAP( VK_SCROLL, TEXT("ScrollLock") );

		ADDKEYMAP( AKEYCODE_SHIFT_LEFT, TEXT("LeftShift") );
		ADDKEYMAP( AKEYCODE_SHIFT_LEFT, TEXT("RightShift") );
		//ADDKEYMAP( VK_LCONTROL, TEXT("LeftControl") );
		//ADDKEYMAP( VK_RCONTROL, TEXT("RightControl") );
		ADDKEYMAP( AKEYCODE_ALT_LEFT, TEXT("LeftAlt") );
		ADDKEYMAP( AKEYCODE_ALT_RIGHT, TEXT("RightAlt") );
		//ADDKEYMAP( VK_LWIN, TEXT("LeftCommand") );
		//ADDKEYMAP( VK_RWIN, TEXT("RightCommand") );

		ADDKEYMAP(AKEYCODE_BACK, TEXT("Android_Back"));
		ADDKEYMAP(AKEYCODE_VOLUME_UP, TEXT("Android_Volume_Up"));
		ADDKEYMAP(AKEYCODE_VOLUME_DOWN, TEXT("Android_Volume_Down"));
		ADDKEYMAP(AKEYCODE_MENU, TEXT("Android_Menu"));

	}

	check(NumMappings < MaxMappings);
	return NumMappings;
#undef ADDKEYMAP
}

bool FAndroidMisc::GetVolumeButtonsHandledBySystem()
{
	return VolumeButtonsHandledBySystem;
}

void FAndroidMisc::SetVolumeButtonsHandledBySystem(bool enabled)
{
	VolumeButtonsHandledBySystem = enabled;
}

void FAndroidMisc::ResetGamepadAssignments()
{
	FAndroidInputInterface::ResetGamepadAssignments();
}

void FAndroidMisc::ResetGamepadAssignmentToController(int32 ControllerId)
{
	FAndroidInputInterface::ResetGamepadAssignmentToController(ControllerId);
}

bool FAndroidMisc::IsControllerAssignedToGamepad(int32 ControllerId)
{
	return FAndroidInputInterface::IsControllerAssignedToGamepad(ControllerId);
}

int32 FAndroidMisc::GetAndroidBuildVersion()
{
	if (AndroidBuildVersion > 0)
	{
		return AndroidBuildVersion;
	}
	if (AndroidBuildVersion <= 0)
	{
		JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
		if (nullptr != JEnv)
		{
			jclass Class = FAndroidApplication::FindJavaClass("com/epicgames/ue4/GameActivity");
			if (nullptr != Class)
			{
				jfieldID Field = JEnv->GetStaticFieldID(Class, "ANDROID_BUILD_VERSION", "I");
				if (nullptr != Field)
				{
					AndroidBuildVersion = JEnv->GetStaticIntField(Class, Field);
				}
				JEnv->DeleteLocalRef(Class);
			}
		}
	}
	return AndroidBuildVersion;
}

extern bool AndroidThunkCpp_GetMetaDataBoolean(const FString& Key);

bool FAndroidMisc::ShouldUseVulkan()
{
#if PLATFORM_ANDROID_VULKAN
	// just do this check once
	static int32 ShouldUseVulkanFlag = -1;
	if (ShouldUseVulkanFlag == -1)
	{
		// assume no
		ShouldUseVulkanFlag = 0;

		// make sure the project setting has enabled Vulkan support (per-project user settings in the editor) from AndroidManifest.xml
		bool bSupportsVulkan = AndroidThunkCpp_GetMetaDataBoolean(TEXT("com.epicgames.ue4.GameActivity.bSupportsVulkan"));
		if (bSupportsVulkan)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("Compiled with Vulkan support"));

			// does commandline override (using GL or ES2 for legacy commandlines)
			bool bForceOpenGL = FParse::Param(FCommandLine::Get(), TEXT("GL")) || FParse::Param(FCommandLine::Get(), TEXT("OpenGL")) || FParse::Param(FCommandLine::Get(), TEXT("ES2"));
			if (!bForceOpenGL)
			{
				// check for libvulkan_sec.so or libvulkan.so for detection
				void* Lib = dlopen("libvulkan_sec.so", 0);
				if (Lib != NULL)
				{
					ShouldUseVulkanFlag = 1;
					FPlatformMisc::LowLevelOutputDebugString(TEXT("Vulkan library detected, using Vulkan"));
				}
				else
				{
					Lib = dlopen("libvulkan.so", 0);
					if (Lib != NULL)
					{
						ShouldUseVulkanFlag = 1;
						FPlatformMisc::LowLevelOutputDebugString(TEXT("Vulkan library detected, using Vulkan"));
					}
					else
					{
						FPlatformMisc::LowLevelOutputDebugString(TEXT("Vulkan library NOT detected, falling back to OpenGL ES"));
					}
				}
			}
			else
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("Forced OpenGL ES"));
			}
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("Compiled with OpenGL ES support"));
		}
	}

	return ShouldUseVulkanFlag ==1;
#else
	return false;
#endif
}

#if !UE_BUILD_SHIPPING
bool FAndroidMisc::IsDebuggerPresent()
{
	bool Result = false;
#if 0
	JNIEnv* JEnv = FAndroidApplication::GetJavaEnv();
	if (nullptr != JEnv)
	{
		jclass Class = FAndroidApplication::FindJavaClass("android/os/Debug");
		if (nullptr != Class)
		{
			// This segfaults for some reason. So this is all disabled for now.
			jmethodID Method = JEnv->GetStaticMethodID(Class, "isDebuggerConnected", "()Z");
			if (nullptr != Method)
			{
				Result = JEnv->CallStaticBooleanMethod(Class, Method);
			}
		}
	}
#endif
	return Result;
}
#endif

int FAndroidMisc::GetVolumeState(double* OutTimeOfChangeInSec)
{
	int v;
	ReceiversLock.Lock();
	v = CurrentVolume.Volume;
	if (OutTimeOfChangeInSec)
	{
		*OutTimeOfChangeInSec = CurrentVolume.TimeOfChange;
	}
	ReceiversLock.Unlock();
	return v;
}

FAndroidMisc::FBatteryState FAndroidMisc::GetBatteryState()
{
	FBatteryState CurState;
	ReceiversLock.Lock();
	CurState = CurrentBatteryState;
	ReceiversLock.Unlock();
	return CurState;
}

bool FAndroidMisc::AreHeadPhonesPluggedIn()
{
	return HeadPhonesArePluggedIn;
}

