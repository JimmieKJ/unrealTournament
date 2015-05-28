// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "AndroidWindow.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

// Cached calculated screen resolution
static int32 WindowWidth = -1;
static int32 WindowHeight = -1;
static bool WindowInit = false;
static float ContentScaleFactor = -1.0f;
static ANativeWindow* LastWindow = NULL;

FAndroidWindow::~FAndroidWindow()
{
	//       Use NativeWindow_Destroy() instead.
}

TSharedRef<FAndroidWindow> FAndroidWindow::Make()
{
	return MakeShareable( new FAndroidWindow() );
}

FAndroidWindow::FAndroidWindow() :Window(NULL)
{
}

void FAndroidWindow::Initialize( class FAndroidApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FAndroidWindow >& InParent, const bool bShowImmediately )
{
	//set window here.

	OwningApplication = Application;
	Definition = InDefinition;
	Window = static_cast<ANativeWindow*>(FPlatformMisc::GetHardwareWindow());
}

bool FAndroidWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();

	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;

	return true;
}


void FAndroidWindow::SetOSWindowHandle(void* InWindow)
{
	Window = static_cast<ANativeWindow*>(InWindow);
}


//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeSetObbInfo(String PackageName, int Version, int PatchVersion);"
static bool GAndroidIsPortrait = false;
static int GAndroidDepthBufferPreference = 0;
extern "C" void Java_com_epicgames_ue4_GameActivity_nativeSetWindowInfo(JNIEnv* jenv, jobject thiz, jboolean bIsPortrait, jint DepthBufferPreference)
{
	GAndroidIsPortrait = bIsPortrait == JNI_TRUE;
	GAndroidDepthBufferPreference = DepthBufferPreference;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("App is running in %s\n"), GAndroidIsPortrait ? TEXT("Portrait") : TEXT("Landscape"));
}

int32 FAndroidWindow::GetDepthBufferPreference()
{
	return GAndroidDepthBufferPreference;
}

FPlatformRect FAndroidWindow::GetScreenRect()
{
	// CSF is a multiplier to 1280x720
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
	float RequestedContentScaleFactor = CVar->GetFloat();

	ANativeWindow* Window = (ANativeWindow*)FPlatformMisc::GetHardwareWindow();
	check(Window != NULL);

	if (RequestedContentScaleFactor != ContentScaleFactor)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** RequestedContentScaleFactor different %d != %d, not using res cache"), RequestedContentScaleFactor, ContentScaleFactor);
	}

	if (Window != LastWindow)
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("***** Window different, not using res cache"));
	}

	if (WindowWidth <= 8)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("***** WindowWidth is %d, not using res cache"), WindowWidth);
	}

	// since orientation won't change on Android, use cached results if still valid
	if (WindowInit && RequestedContentScaleFactor == ContentScaleFactor && Window == LastWindow && WindowWidth > 8)
	{
		FPlatformRect ScreenRect;
		ScreenRect.Left = 0;
		ScreenRect.Top = 0;
		ScreenRect.Right = WindowWidth;
		ScreenRect.Bottom = WindowHeight;

		return ScreenRect;
	}

	// currently hardcoding resolution

	// get the aspect ratio of the physical screen
	int32 ScreenWidth, ScreenHeight;
	CalculateSurfaceSize(Window, ScreenWidth, ScreenHeight);
	float AspectRatio = (float)ScreenWidth / (float)ScreenHeight;

	int32 MaxWidth = ScreenWidth; 
	int32 MaxHeight = ScreenHeight;

	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	static auto* MobileHDR32bppCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR32bpp"));
	const bool bMobileHDR32bpp = (MobileHDRCvar && MobileHDRCvar->GetValueOnAnyThread() == 1)
		&& (FAndroidMisc::SupportsFloatingPointRenderTargets() == false || (MobileHDR32bppCvar && MobileHDR32bppCvar->GetValueOnAnyThread() == 1));

	UE_LOG(LogAndroid, Log, TEXT("Requires Mosaic: %s"), bMobileHDR32bpp ? TEXT("YES") : TEXT("no"));

	if (bMobileHDR32bpp)
	{
		const int32 OldMaxWidth = MaxWidth;
		const int32 OldMaxHeight = MaxHeight;

		if (GAndroidIsPortrait)
		{
			MaxHeight = FPlatformMath::Min(MaxHeight,1024);
			MaxWidth = MaxHeight * AspectRatio;
		}
		else
		{
			MaxWidth = FPlatformMath::Min(MaxWidth,1024);
			MaxHeight = MaxWidth / AspectRatio;
		}

		UE_LOG(LogAndroid, Log, TEXT("Limiting MaxWidth=%d and MaxHeight=%d due to bMobileHDR32bpp (was %dx%d)"), MaxWidth, MaxHeight, OldMaxWidth, OldMaxHeight);
	}

	// 0 means to use native size
	int32 Width, Height;
	if (RequestedContentScaleFactor == 0.0f)
	{
		Width = MaxWidth;
		Height = MaxHeight;
		UE_LOG(LogAndroid, Log, TEXT("Setting Width=%d and Height=%d (requested scale = 0 = auto)"), Width, Height);
	}
	else
	{
		if (GAndroidIsPortrait)
		{
			Height = 1280 * RequestedContentScaleFactor;
		}
		else
		{
			Height = 720 * RequestedContentScaleFactor;
		}

		// apply the aspect ration to get the width
		Width = Height * AspectRatio;

		// clamp to native resolution
		Width = FPlatformMath::Min(Width, MaxWidth);
		Height = FPlatformMath::Min(Height, MaxHeight);

		UE_LOG(LogAndroid, Log, TEXT("Setting Width=%d and Height=%d (requested scale = %f)"), Width, Height, RequestedContentScaleFactor);
	}

	FPlatformRect ScreenRect;
	ScreenRect.Left = 0;
	ScreenRect.Top = 0;
	ScreenRect.Right = Width;
	ScreenRect.Bottom = Height;

	// save for future calls
	WindowWidth = Width;
	WindowHeight = Height;
	WindowInit = true;
	ContentScaleFactor = RequestedContentScaleFactor;
	LastWindow = Window;

	return ScreenRect;
}


void FAndroidWindow::CalculateSurfaceSize(void* InWindow, int32_t& SurfaceWidth, int32_t& SurfaceHeight)
{
	check(InWindow);
	ANativeWindow* Window = (ANativeWindow*)InWindow;

	SurfaceWidth =  ANativeWindow_getWidth(Window);
	SurfaceHeight = ANativeWindow_getHeight(Window);

	// some phones gave it the other way (so, if swap if the app is landscape, but width < height)
	if (!GAndroidIsPortrait && SurfaceWidth < SurfaceHeight)
	{
		Swap(SurfaceWidth, SurfaceHeight);
	}

	// ensure the size is divisible by a specified amount
	const int DividableBy = 8;
	SurfaceWidth  = ((SurfaceWidth  + DividableBy - 1) / DividableBy) * DividableBy;
	SurfaceHeight = ((SurfaceHeight + DividableBy - 1) / DividableBy) * DividableBy;
}
