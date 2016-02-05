// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "SlateCore.h"
#include "WebBrowserSingleton.h"
#include "WebBrowserApp.h"
#include "WebBrowserHandler.h"
#include "WebBrowserWindow.h"
#include "IPlatformTextField.h"
#include "IVirtualKeyboardEntry.h"
#include "SlateApplication.h"
#include "Runtime/Launch/Resources/Version.h"

#if WITH_CEF3
#	if PLATFORM_WINDOWS
#		include "AllowWindowsPlatformTypes.h"
#	endif
#	pragma push_macro("OVERRIDE")
#		undef OVERRIDE // cef headers provide their own OVERRIDE macro
#		include "include/cef_app.h"
#	pragma pop_macro("OVERRIDE")
#	if PLATFORM_WINDOWS
#		include "HideWindowsPlatformTypes.h"
#	endif
#endif

#if PLATFORM_MAC || PLATFORM_LINUX
#	include <pthread.h>
#endif

#if PLATFORM_ANDROID
#	include <Android/AndroidPlatformWebBrowser.h>
#endif

// Define some platform-dependent file locations
#if WITH_CEF3
#	define CEF3_BIN_DIR TEXT("Binaries/ThirdParty/CEF3")
#	if PLATFORM_WINDOWS && PLATFORM_64BITS
#		define CEF3_RESOURCES_DIR CEF3_BIN_DIR TEXT("/Win64/Resources")
#		define CEF3_SUBPROCES_EXE TEXT("Binaries/Win64/UnrealCEFSubProcess.exe")
#	elif PLATFORM_WINDOWS && PLATFORM_32BITS
#		define CEF3_RESOURCES_DIR CEF3_BIN_DIR TEXT("/Win32/Resources")
#		define CEF3_SUBPROCES_EXE TEXT("Binaries/Win32/UnrealCEFSubProcess.exe")
#	elif PLATFORM_MAC
#		define CEF3_FRAMEWORK_DIR CEF3_BIN_DIR TEXT("/Mac/Chromium Embedded Framework.framework")
#		define CEF3_RESOURCES_DIR CEF3_FRAMEWORK_DIR TEXT("/Resources")
#		define CEF3_SUBPROCES_EXE TEXT("Binaries/Mac/UnrealCEFSubProcess.app/Contents/MacOS/UnrealCEFSubProcess")
#	elif PLATFORM_LINUX // @todo Linux
#		define CEF3_RESOURCES_DIR CEF3_BIN_DIR TEXT("/Linux/Resources")
#		define CEF3_SUBPROCES_EXE TEXT("Binaries/Linux/UnrealCEFSubProcess")
#	endif
#endif

namespace {

	/**
	 * Helper function to set the current thread name, visible by the debugger.
	 * @param ThreadName	Name to set
	 */
	void SetCurrentThreadName(char* ThreadName)
	{
#if PLATFORM_MAC
		pthread_setname_np(ThreadName);
#elif PLATFORM_LINUX
		pthread_setname_np(pthread_self(), ThreadName);
#elif PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
		/**
		 * Code setting the thread name for use in the debugger.
		 * Copied implementation from WindowsRunnableThread as it is private.
		 *
		 * http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
		 */
		const uint32 MS_VC_EXCEPTION=0x406D1388;

		struct THREADNAME_INFO
		{
			uint32 dwType;		// Must be 0x1000.
			LPCSTR szName;		// Pointer to name (in user addr space).
			uint32 dwThreadID;	// Thread ID (-1=caller thread).
			uint32 dwFlags;		// Reserved for future use, must be zero.
		};

		THREADNAME_INFO ThreadNameInfo = {0x1000, ThreadName, -1, 0};

		__try
		{
			RaiseException( MS_VC_EXCEPTION, 0, sizeof(ThreadNameInfo)/sizeof(ULONG_PTR), (ULONG_PTR*)&ThreadNameInfo );
		}
		__except( EXCEPTION_EXECUTE_HANDLER )
		CA_SUPPRESS(6322)
		{	
		}
#endif
	}

#if PLATFORM_MAC
	// OSX wants caches in a separate location from other app data
	const TCHAR* ApplicationCacheDir()
	{
		static TCHAR Result[MAX_PATH] = TEXT("");
		if (!Result[0])
		{
			SCOPED_AUTORELEASE_POOL;
			NSString *CacheBaseDir = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) objectAtIndex: 0];
			NSString* BundleID = [[NSBundle mainBundle] bundleIdentifier];
			if(!BundleID)
			{
				BundleID = [[NSProcessInfo processInfo] processName];
			}
			check(BundleID);
			
			NSString* AppCacheDir = [CacheBaseDir stringByAppendingPathComponent: BundleID];
			FPlatformString::CFStringToTCHAR((CFStringRef)AppCacheDir, Result);
		}
		return Result;
	}
#endif
}

FWebBrowserSingleton::FWebBrowserSingleton()
	: bDevToolsShortcutEnabled(UE_BUILD_DEBUG)
{
#if WITH_CEF3
	// The FWebBrowserSingleton must be initialized on the game thread
	check(IsInGameThread());

	// Provide CEF with command-line arguments.
#if PLATFORM_WINDOWS
	CefMainArgs MainArgs(hInstance);
#else
	CefMainArgs MainArgs;
#endif

	// WebBrowserApp implements application-level callbacks.
	WebBrowserApp = new FWebBrowserApp;
	WebBrowserApp->OnRenderProcessThreadCreated().BindRaw(this, &FWebBrowserSingleton::HandleRenderProcessCreated);

	// Specify CEF global settings here.
	CefSettings Settings;
	Settings.no_sandbox = true;
	Settings.command_line_args_disabled = true;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Settings.remote_debugging_port = 8088;
#endif

	FString CefLogFile(FPaths::Combine(*FPaths::GameLogDir(), TEXT("cef3.log")));
	CefLogFile = FPaths::ConvertRelativePathToFull(CefLogFile);
	CefString(&Settings.log_file) = *CefLogFile;
	Settings.log_severity = LOGSEVERITY_WARNING;

	// Specify locale from our settings
	FString LocaleCode = GetCurrentLocaleCode();
	CefString(&Settings.locale) = *LocaleCode;

	// Append engine version to the user agent string.
	FString ProductVersion = FString::Printf( TEXT("%s UnrealEngineChrome/%s"), FApp::GetGameName(), ENGINE_VERSION_STRING);
	CefString(&Settings.product_version) = *ProductVersion;

	// Enable on disk cache
#if PLATFORM_MAC
	// OSX wants cache files in a special location
	FString CachePath(FPaths::Combine(ApplicationCacheDir(), TEXT("webcache")));
#else
	FString CachePath(FPaths::Combine(*FPaths::GameSavedDir(), TEXT("webcache")));
#endif
	CachePath = FPaths::ConvertRelativePathToFull(CachePath);
	CefString(&Settings.cache_path) = *CachePath;

	// Specify path to resources
	FString ResourcesPath(FPaths::Combine(*FPaths::EngineDir(), CEF3_RESOURCES_DIR));
	ResourcesPath = FPaths::ConvertRelativePathToFull(ResourcesPath);
	if (!FPaths::DirectoryExists(ResourcesPath))
	{
		UE_LOG(LogWebBrowser, Error, TEXT("Chromium Resources information not found at: %s."), *ResourcesPath);
	}
	CefString(&Settings.resources_dir_path) = *ResourcesPath;

#if !PLATFORM_MAC
	// On Mac Chromium ignores custom locales dir. Files need to be stored in Resources folder in the app bundle
	FString LocalesPath(FPaths::Combine(*ResourcesPath, TEXT("locales")));
	LocalesPath = FPaths::ConvertRelativePathToFull(LocalesPath);
	if (!FPaths::DirectoryExists(LocalesPath))
	{
		UE_LOG(LogWebBrowser, Error, TEXT("Chromium Locales information not found at: %s."), *LocalesPath);
	}
	CefString(&Settings.locales_dir_path) = *LocalesPath;
#endif

	// Specify path to sub process exe
	FString SubProcessPath(FPaths::Combine(*FPaths::EngineDir(), CEF3_SUBPROCES_EXE));
	SubProcessPath = FPaths::ConvertRelativePathToFull(SubProcessPath);
	if (!FPaths::FileExists(SubProcessPath))
	{
		UE_LOG(LogWebBrowser, Error, TEXT("UnrealCEFSubProcess.exe not found, check that this program has been built and is placed in: %s."), *SubProcessPath);
	}
	CefString(&Settings.browser_subprocess_path) = *SubProcessPath;

	// Initialize CEF.
	bool bSuccess = CefInitialize(MainArgs, Settings, WebBrowserApp.get(), nullptr);
	check(bSuccess);

	// Set the thread name back to GameThread.
	SetCurrentThreadName(TCHAR_TO_ANSI( *(FName( NAME_GameThread ).GetPlainNameString()) ));
#endif
}

#if WITH_CEF3
void FWebBrowserSingleton::HandleRenderProcessCreated(CefRefPtr<CefListValue> ExtraInfo)
{
	for (int32 Index = WindowInterfaces.Num() - 1; Index >= 0; --Index)
	{
		TSharedPtr<FWebBrowserWindow> BrowserWindow = WindowInterfaces[Index].Pin();
		if (BrowserWindow.IsValid())
		{
			CefRefPtr<CefDictionaryValue> Bindings = BrowserWindow->GetProcessInfo();
			if (Bindings.get())
			{
				ExtraInfo->SetDictionary(ExtraInfo->GetSize(), Bindings);
			}
		}
	}
}
#endif

FWebBrowserSingleton::~FWebBrowserSingleton()
{
#if WITH_CEF3
	// Force all existing browsers to close in case any haven't been deleted
	for (int32 Index = 0; Index < WindowInterfaces.Num(); ++Index)
	{
		if (WindowInterfaces[Index].IsValid())
		{
			WindowInterfaces[Index].Pin()->CloseBrowser(true);
		}
	}

	// Just in case, although we deallocate WebBrowserApp right after this.
	WebBrowserApp->OnRenderProcessThreadCreated().Unbind();
	// CefRefPtr takes care of delete
	WebBrowserApp = nullptr;
	// Shut down CEF.
	CefShutdown();

#endif
}

TSharedPtr<IWebBrowserWindow> FWebBrowserSingleton::CreateBrowserWindow(
	TSharedPtr<FWebBrowserWindow>& BrowserWindowParent,
	TSharedPtr<FWebBrowserWindowInfo>& BrowserWindowInfo
	)
{
#if WITH_CEF3

	TOptional<FString> ContentsToLoad;

	bool bShowErrorMessage = BrowserWindowParent->IsShowingErrorMessages();
	bool bThumbMouseButtonNavigation = BrowserWindowParent->IsThumbMouseButtonNavigationEnabled();
	bool bUseTransparency = BrowserWindowParent->UseTransparency();
	FString InitialURL = BrowserWindowInfo->Browser->GetMainFrame()->GetURL().ToWString().c_str();
	TSharedPtr<FWebBrowserWindow> NewBrowserWindow(new FWebBrowserWindow(BrowserWindowInfo->Browser, InitialURL, ContentsToLoad, bShowErrorMessage, bThumbMouseButtonNavigation, bUseTransparency));
	BrowserWindowInfo->Handler->SetBrowserWindow(NewBrowserWindow);

	WindowInterfaces.Add(NewBrowserWindow);
	return NewBrowserWindow;
#endif
	return nullptr;
}

TSharedPtr<IWebBrowserWindow> FWebBrowserSingleton::CreateBrowserWindow(
	void* OSWindowHandle, 
	FString InitialURL, 
	bool bUseTransparency,
	bool bThumbMouseButtonNavigation,
	TOptional<FString> ContentsToLoad, 
	bool ShowErrorMessage,
	FColor BackgroundColor)
{
#if WITH_CEF3
	static bool AllowCEF = !FParse::Param(FCommandLine::Get(), TEXT("nocef"));
	if (AllowCEF)
	{
		// Information used when creating the native window.
		CefWindowHandle WindowHandle = (CefWindowHandle)OSWindowHandle; // TODO: check this is correct for all platforms
		CefWindowInfo WindowInfo;

		// Always use off screen rendering so we can integrate with our windows
		WindowInfo.SetAsWindowless(WindowHandle, bUseTransparency);

		// Specify CEF browser settings here.
		CefBrowserSettings BrowserSettings;

		// Set max framerate to maximum supported.
		BrowserSettings.windowless_frame_rate = 60;
		BrowserSettings.background_color = CefColorSetARGB(BackgroundColor.A, BackgroundColor.R, BackgroundColor.G, BackgroundColor.B);

		// Disable plugins
		BrowserSettings.plugins = STATE_DISABLED;


		// WebBrowserHandler implements browser-level callbacks.
		CefRefPtr<FWebBrowserHandler> NewHandler(new FWebBrowserHandler);

		// Create the CEF browser window.
		CefRefPtr<CefBrowser> Browser = CefBrowserHost::CreateBrowserSync(WindowInfo, NewHandler.get(), *InitialURL, BrowserSettings, nullptr);
		if (Browser.get())
		{
			// Create new window
			TSharedPtr<FWebBrowserWindow> NewBrowserWindow(new FWebBrowserWindow(Browser, InitialURL, ContentsToLoad, ShowErrorMessage, bThumbMouseButtonNavigation, bUseTransparency));
			NewHandler->SetBrowserWindow(NewBrowserWindow);

			WindowInterfaces.Add(NewBrowserWindow);
			return NewBrowserWindow;
		}
	}
#elif PLATFORM_ANDROID
	// Create new window
	TSharedPtr<FWebBrowserWindow> NewBrowserWindow = MakeShareable(new FWebBrowserWindow(InitialURL, ContentsToLoad, ShowErrorMessage, bThumbMouseButtonNavigation, bUseTransparency));

	//WindowInterfaces.Add(NewBrowserWindow);
	return NewBrowserWindow;
#endif
	return nullptr;
}

bool FWebBrowserSingleton::Tick(float DeltaTime)
{
#if WITH_CEF3
	bool bIsSlateAwake = !FSlateApplication::Get().IsSlateAsleep();
	// Remove any windows that have been deleted and check wether it's currently visible
	for (int32 Index = WindowInterfaces.Num() - 1; Index >= 0; --Index)
	{
		if (!WindowInterfaces[Index].IsValid())
		{
			WindowInterfaces.RemoveAtSwap(Index);
		}
		else if (bIsSlateAwake) // only check for Tick activity if Slate is currently ticking
		{
			TSharedPtr<FWebBrowserWindow> BrowserWindow = WindowInterfaces[Index].Pin();
			if(BrowserWindow.IsValid())
			{
				// Test if we've ticked recently. If not assume the browser window has become hidden.
				BrowserWindow->CheckTickActivity();
			}
		}
	}
	CefDoMessageLoopWork();
#endif
	return true;
}

FString FWebBrowserSingleton::GetCurrentLocaleCode()
{
	FCultureRef Culture = FInternationalization::Get().GetCurrentCulture();
	FString LocaleCode = Culture->GetTwoLetterISOLanguageName();
	FString Country = Culture->GetRegion();
	if (!Country.IsEmpty())
	{
		LocaleCode = LocaleCode + TEXT("-") + Country;
	}
	return LocaleCode;
}

#if WITH_CEF3
namespace
{
	// Task for executing a callback on a given thread.
	class FDeleteCookiesNotificationTask
		: public CefTask
	{
		TFunction<void (int)> Callback;
		int Value;
	public:
		FDeleteCookiesNotificationTask(const TFunction<void (int)>& InCallback, int InValue)
			: Callback(InCallback)
			, Value(InValue)
		{}

		virtual void Execute() override
		{
			Callback(Value);
		}

		IMPLEMENT_REFCOUNTING(FDeleteCookiesNotificationTask);

	};

	// Callback that will invoke the callback with the result on the UI thread.
	class FDeleteCookiesFunctionCallback
		: public CefDeleteCookiesCallback
	{
		TFunction<void (int)> Callback;
	public:
		FDeleteCookiesFunctionCallback(const TFunction<void (int)>& InCallback)
			: Callback(InCallback)
		{}

		virtual void OnComplete(int NumDeleted) override
		{
			// We're on the IO thread, so we'll have to schedule the callback on the main thread
			CefPostTask(TID_UI, new FDeleteCookiesNotificationTask(Callback, NumDeleted));
		}

		IMPLEMENT_REFCOUNTING(FDeleteCookiesFunctionCallback);
	};
}
#endif

void FWebBrowserSingleton::DeleteBrowserCookies(FString URL, FString CookieName, TFunction<void (int)> Completed)
{
#if WITH_CEF3
	CefRefPtr<FDeleteCookiesFunctionCallback> Callback = Completed ? new FDeleteCookiesFunctionCallback(Completed) : nullptr;
	CefCookieManager::GetGlobalManager(nullptr)->DeleteCookies(*URL, *CookieName, Callback);
#endif
}


// Cleanup macros to avoid having them leak outside this source file
#undef CEF3_BIN_DIR
#undef CEF3_FRAMEWORK_DIR
#undef CEF3_RESOURCES_DIR
#undef CEF3_SUBPROCES_EXE
