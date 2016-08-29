// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogApp, Log, All);

/* FApp static initialization
 *****************************************************************************/

FGuid FApp::InstanceId = FGuid::NewGuid();
FGuid FApp::SessionId = FGuid::NewGuid();
FString FApp::SessionName = FString();
FString FApp::SessionOwner = FString();
TArray<FString> FApp::SessionUsers = TArray<FString>();
bool FApp::Standalone = true;
bool FApp::bIsBenchmarking = false;
bool FApp::bUseFixedSeed = false;
bool FApp::bUseFixedTimeStep = false;
double FApp::FixedDeltaTime = 1 / 30.0;
double FApp::CurrentTime = 0.0;
double FApp::LastTime = 0.0;
double FApp::DeltaTime = 1 / 30.0;
double FApp::IdleTime = 0.0;
float FApp::VolumeMultiplier = 1.0f;
float FApp::UnfocusedVolumeMultiplier = 0.0f;
bool FApp::bUseVRFocus = false;
bool FApp::bHasVRFocus = false;


/* FApp static interface
 *****************************************************************************/

FString FApp::GetBranchName()
{
	return FString(TEXT(BRANCH_NAME));
}

const TCHAR* FApp::GetBuildVersion()
{
	return BUILD_VERSION;
}

int32 FApp::GetEngineIsPromotedBuild()
{
	return ENGINE_IS_PROMOTED_BUILD;
}


FString FApp::GetEpicProductIdentifier()
{
	return FString(TEXT(EPIC_PRODUCT_IDENTIFIER));
}

const TCHAR * FApp::GetDeploymentName()
{
	static TCHAR StaticDeploymentName[64] = {0};
	static bool bHaveDeployment = false;

	if (!bHaveDeployment)
	{
		// use -epicapp value from the commandline. Default deployment is not captured by this,
		// but this may not be a problem as that would be the case only during the development
		FParse::Value(FCommandLine::Get(), TEXT("EPICAPP="), StaticDeploymentName, ARRAY_COUNT(StaticDeploymentName) - 1);
		bHaveDeployment = true;
	}

	return StaticDeploymentName;
}

EBuildConfigurations::Type FApp::GetBuildConfiguration()
{
#if UE_BUILD_DEBUG
	return EBuildConfigurations::Debug;

#elif UE_BUILD_DEVELOPMENT
	// Detect DebugGame using an extern variable in monolithic configurations, or a command line argument in modular configurations.
	#if IS_MONOLITHIC
		extern const bool GIsDebugGame;
		return GIsDebugGame? EBuildConfigurations::DebugGame : EBuildConfigurations::Development;
	#else
		static const bool bUsingDebugGame = FParse::Param(FCommandLine::Get(), TEXT("debug"));
		return bUsingDebugGame? EBuildConfigurations::DebugGame : EBuildConfigurations::Development;
	#endif

#elif UE_BUILD_SHIPPING || UI_BUILD_SHIPPING_EDITOR
	return EBuildConfigurations::Shipping;

#elif UE_BUILD_TEST
	return EBuildConfigurations::Test;

#else
	return EBuildConfigurations::Unknown;
#endif
}


FString FApp::GetBuildDate()
{
	return FString(ANSI_TO_TCHAR(__DATE__));
}


void FApp::InitializeSession()
{
	// parse session details on command line
	FString InstanceIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-InstanceId="), InstanceIdString))
	{
		if (!FGuid::Parse(InstanceIdString, InstanceId))
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
		}
	}

	if (!InstanceId.IsValid())
	{
		InstanceId = FGuid::NewGuid();
	}

	FString SessionIdString;

	if (FParse::Value(FCommandLine::Get(), TEXT("-SessionId="), SessionIdString))
	{
		if (FGuid::Parse(SessionIdString, SessionId))
		{
			Standalone = false;
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT("Invalid SessionId on command line: %s"), *SessionIdString);
		}
	}

	FParse::Value(FCommandLine::Get(), TEXT("-SessionName="), SessionName);

	if (!FParse::Value(FCommandLine::Get(), TEXT("-SessionOwner="), SessionOwner))
	{
		SessionOwner = FPlatformProcess::UserName(false);
	}
}


bool FApp::IsInstalled()
{
#if UE_BUILD_SHIPPING && PLATFORM_DESKTOP && !UE_SERVER
	static bool bIsInstalled = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalled"));
#else
	static bool bIsInstalled = FParse::Param(FCommandLine::Get(), TEXT("Installed"));
#endif
	return bIsInstalled;
}


bool FApp::IsEngineInstalled()
{
	static int32 EngineInstalledState = -1;

	if (EngineInstalledState == -1)
	{
		bool bIsInstalledEngine = IsInstalled();
		FString InstalledBuildFile = FPaths::RootDir() / TEXT("Engine/Build/InstalledBuild.txt");
		FPaths::NormalizeFilename(InstalledBuildFile);
		bIsInstalledEngine |= IFileManager::Get().FileExists(*InstalledBuildFile);
		// Allow commandline options to disable/enable installed engine behavior
		if (bIsInstalledEngine)
		{
			bIsInstalledEngine = !FParse::Param(FCommandLine::Get(), TEXT("NotInstalledEngine"));
		}
		else
		{
			bIsInstalledEngine = FParse::Param(FCommandLine::Get(), TEXT("InstalledEngine"));
		}
		EngineInstalledState = bIsInstalledEngine ? 1 : 0;
	}

	return EngineInstalledState == 1;
}

#if PLATFORM_WINDOWS && defined(__clang__)
bool FApp::IsUnattended() // @todo clang: Workaround for missing symbol export
{
	static bool bIsUnattended = FParse::Param(FCommandLine::Get(), TEXT("UNATTENDED"));
	return bIsUnattended || GIsAutomationTesting;
}
#endif

#if HAVE_RUNTIME_THREADING_SWITCHES
bool FApp::ShouldUseThreadingForPerformance()
{
	static bool OnlyOneThread = FParse::Param(FCommandLine::Get(), TEXT("ONETHREAD")) || IsRunningDedicatedServer() || !FPlatformProcess::SupportsMultithreading() || FPlatformMisc::NumberOfCores() < 2;
	return !OnlyOneThread;
}
#endif // HAVE_RUNTIME_THREADING_SWITCHES


static bool GUnfocusedVolumeMultiplierInitialised = false;
float FApp::GetUnfocusedVolumeMultiplier()
{
	if (!GUnfocusedVolumeMultiplierInitialised)
	{
		GUnfocusedVolumeMultiplierInitialised = true;
		GConfig->GetFloat(TEXT("Audio"), TEXT("UnfocusedVolumeMultiplier"), UnfocusedVolumeMultiplier, GEngineIni);
	}
	return UnfocusedVolumeMultiplier;
}

void FApp::SetUnfocusedVolumeMultiplier(float InVolumeMultiplier)
{
	UnfocusedVolumeMultiplier = InVolumeMultiplier;
	GUnfocusedVolumeMultiplierInitialised = true;
}

void FApp::SetUseVRFocus(bool bInUseVRFocus)
{
	bUseVRFocus = bInUseVRFocus;
}

void FApp::SetHasVRFocus(bool bInHasVRFocus)
{
	bHasVRFocus = bInHasVRFocus;
}
