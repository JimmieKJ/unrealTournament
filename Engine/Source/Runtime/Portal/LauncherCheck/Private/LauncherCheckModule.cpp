// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LauncherCheckPrivatePCH.h"
#include "GenericPlatformHttp.h"

#if WITH_LAUNCHERCHECK

#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"

#endif

/**
 * Log categories for LauncherCheck module
 */
DEFINE_LOG_CATEGORY(LogLauncherCheck);

/**
 * Implements the Launcher Check module.
 */
class FLauncherCheckModule
	: public ILauncherCheckModule
{
public:

	// ILauncherCheckModule interface

	virtual bool WasRanFromLauncher() const override
	{
		// Check for the presence of a specific param that's passed from the Launcher to the games
		// when we want to make sure we've come from the Launcher
		return !IsEnabled() || FParse::Param(FCommandLine::Get(), TEXT("EpicPortal"));
	}

	virtual bool RunLauncher() const override
	{
#if WITH_LAUNCHERCHECK
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

		if (DesktopPlatform != nullptr)
		{
			static const TCHAR* Delims[] = { TEXT("/") };
			static const int32 NumDelims = ARRAY_COUNT(Delims);

			// Get the path to the executable, and encode it ('cos spaces, colons, etc break things)
			FString ExePath = FPlatformProcess::BaseDir();

			// Make sure it's not relative and the slashes are the right way
			ExePath = FPaths::ConvertRelativePathToFull(ExePath);
			ExePath.ReplaceInline(TEXT("\\"), TEXT("/"));

			// Encode the path 'cos symbols like ':',' ' etc are bad
			FString EncodedExePath;
			TArray<FString> ExeFolders;
			ExePath.ParseIntoArray(ExeFolders, Delims, NumDelims);
			for (const FString& ExeFolder : ExeFolders)
			{
				EncodedExePath /= FGenericPlatformHttp::UrlEncode(ExeFolder);
			}

			// Make sure it ends in a slash
			EncodedExePath /= TEXT("");

			// Construct a url to tell the launcher of this app and what we want to do with it
			FOpenLauncherOptions LauncherOptions;
			LauncherOptions.LauncherRelativeUrl = TEXT("apps");
			LauncherOptions.LauncherRelativeUrl /= EncodedExePath;
			LauncherOptions.LauncherRelativeUrl += TEXT("?action=launch");
			return DesktopPlatform->OpenLauncher(LauncherOptions);
		}
#endif // WITH_LAUNCHERCHECK
		return false;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	/*
	 * Check to see if this module should perform any checks or not
	 * @return true, if it should
	 */
	bool IsEnabled() const
	{
#if WITH_LAUNCHERCHECK
		// Check for the presence of a specific param that's passed from the Launcher to the game
		// when we want to disabled the module
		return !FParse::Param(FCommandLine::Get(), TEXT("NoEpicPortal"));
#else
		return false;
#endif
	}
};


IMPLEMENT_MODULE(FLauncherCheckModule, LauncherCheck );
