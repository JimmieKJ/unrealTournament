// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LaunchPrivatePCH.h"
#include "Internationalization/Internationalization.h"
#include "Ticker.h"
#include "ConsoleManager.h"
#include "ExceptionHandling.h"
#include "FileManagerGeneric.h"
#include "TaskGraphInterfaces.h"
#include "StatsMallocProfilerProxy.h"
#include "Runtime/Core/Public/Modules/ModuleVersion.h"

#include "Projects.h"
#include "UProjectInfo.h"
#include "EngineVersion.h"

#include "ModuleManager.h"

#if WITH_EDITOR
	#include "EditorStyle.h"
	#include "AutomationController.h"
	#include "ProfilerClient.h"
	#include "RemoteConfigIni.h"
	#include "EditorCommandLineUtils.h"

	#if PLATFORM_WINDOWS
		#include "AllowWindowsPlatformTypes.h"
			#include <objbase.h>
		#include "HideWindowsPlatformTypes.h"
	#endif
#endif

#if WITH_ENGINE
	#include "Database.h"
	#include "DerivedDataCacheInterface.h"
	#include "RenderCore.h"
	#include "ShaderCompiler.h"
	#include "ShaderCache.h"
	#include "DistanceFieldAtlas.h"
	#include "GlobalShader.h"
	#include "ParticleHelper.h"
	#include "Online.h"
	#include "PhysicsPublic.h"
	#include "PlatformFeatures.h"
	#include "DeviceProfiles/DeviceProfileManager.h"
	#include "Commandlets/Commandlet.h"
	#include "EngineService.h"
	#include "ContentStreaming.h"
	#include "HighResScreenshot.h"
	#include "HotReloadInterface.h"
	#include "ISessionService.h"
	#include "ISessionServicesModule.h"
	#include "Engine/GameInstance.h"

#if !UE_SERVER
	#include "HeadMountedDisplay.h"
	#include "ISlateRHIRendererModule.h"
	#include "EngineFontServices.h"
#endif

	#include "MoviePlayer.h"

#if !UE_BUILD_SHIPPING
	#include "STaskGraph.h"
	#include "ProfilerService.h"
#endif

#if WITH_AUTOMATION_WORKER
	#include "AutomationWorker.h"
#endif

	/** 
	 *	Function to free up the resources in GPrevPerBoneMotionBlur
	 *	Should only be called at application exit
	 */
	ENGINE_API void MotionBlur_Free();

#endif  //WITH_ENGINE

#if WITH_EDITOR
	#include "FeedbackContextEditor.h"
	static FFeedbackContextEditor UnrealEdWarn;
#endif	// WITH_EDITOR

#if UE_EDITOR
	#include "DesktopPlatformModule.h"
#endif

#define LOCTEXT_NAMESPACE "LaunchEngineLoop"

#if PLATFORM_WINDOWS
	#include "AllowWindowsPlatformTypes.h"
	#include <ObjBase.h>
	#include "HideWindowsPlatformTypes.h"
#endif

#if ENABLE_VISUAL_LOG
#	include "VisualLogger/VisualLogger.h"
#endif

// Pipe output to std output
// This enables UBT to collect the output for it's own use
class FOutputDeviceStdOutput : public FOutputDevice
{
public:

	FOutputDeviceStdOutput()
	{
		bAllowLogVerbosity = FParse::Param(FCommandLine::Get(), TEXT("AllowStdOutLogVerbosity"));
	}

	virtual ~FOutputDeviceStdOutput()
	{
	}

	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override
	{
		if ( (bAllowLogVerbosity && Verbosity <= ELogVerbosity::Log) || (Verbosity <= ELogVerbosity::Display) )
		{
#if PLATFORM_USE_LS_SPEC_FOR_WIDECHAR
			wprintf(TEXT("\n%ls"), *FOutputDevice::FormatLogLine(Verbosity, Category, V, GPrintLogTimes));
#else
			wprintf(TEXT("\n%s"), *FOutputDevice::FormatLogLine(Verbosity, Category, V, GPrintLogTimes));
#endif
			fflush(stdout);
		}
	}

private:
	bool bAllowLogVerbosity;
};

// Exits the game/editor if any of the specified phrases appears in the log output
class FOutputDeviceTestExit : public FOutputDevice
{
	TArray<FString> ExitPhrases;
public:
	FOutputDeviceTestExit(const TArray<FString>& InExitPhrases)
		: ExitPhrases(InExitPhrases)
	{
	}
	virtual ~FOutputDeviceTestExit()
	{
	}
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (!GIsRequestingExit)
		{
			for (auto& Phrase : ExitPhrases)
			{
				if (FCString::Stristr(V, *Phrase) && !FCString::Stristr(V, TEXT("-testexit=")))
				{
#if WITH_ENGINE
					if (GEngine != NULL)
					{
						if (GIsEditor)
						{
							GEngine->DeferredCommands.Add(TEXT("CLOSE_SLATE_MAINFRAME"));					
						}
						else
						{
							GEngine->Exec(NULL, TEXT("QUIT"));
						}
					}
#else
					FPlatformMisc::RequestExit(true);
#endif
					break;
				}
			}
		}
	}
};

static TScopedPointer<FOutputDeviceConsole>	GScopedLogConsole;
static TScopedPointer<FOutputDeviceStdOutput> GScopedStdOut;
static TScopedPointer<FOutputDeviceTestExit> GScopedTestExit;

/**
 * Initializes std out device and adds it to GLog
 **/
void InitializeStdOutDevice()
{
	// Check if something is trying to initialize std out device twice.
	check(!GScopedStdOut.IsValid());

	GScopedStdOut = new FOutputDeviceStdOutput();
	GLog->AddOutputDevice(GScopedStdOut.GetOwnedPointer());
}

bool ParseGameProjectFromCommandLine(const TCHAR* InCmdLine, FString& OutProjectFilePath, FString& OutGameName)
{
	const TCHAR *CmdLine = InCmdLine;
	FString FirstCommandLineToken = FParse::Token(CmdLine, 0);

	// trim any whitespace at edges of string - this can happen if the token was quoted with leading or trailing whitespace
	// VC++ tends to do this in its "external tools" config
	FirstCommandLineToken = FirstCommandLineToken.Trim();

	// 
	OutProjectFilePath = TEXT("");
	OutGameName = TEXT("");

	if ( FirstCommandLineToken.Len() && !FirstCommandLineToken.StartsWith(TEXT("-")) )
	{
		// The first command line argument could be the project file if it exists or the game name if not launching with a project file
		const FString ProjectFilePath = FString(FirstCommandLineToken);
		if ( FPaths::GetExtension(ProjectFilePath) == FProjectDescriptor::GetExtension() )
		{
			OutProjectFilePath = FirstCommandLineToken;
			// Here we derive the game name from the project file
			OutGameName = FPaths::GetBaseFilename(OutProjectFilePath);
			return true;
		}
		else if (FPlatformProperties::IsMonolithicBuild() == false)
		{
			// Full game name is assumed to be the first token
			OutGameName = MoveTemp(FirstCommandLineToken);
			// Derive the project path from the game name. All games must have a uproject file, even if they are in the root folder.
			OutProjectFilePath = FPaths::Combine(*FPaths::RootDir(), *OutGameName, *FString(OutGameName + TEXT(".") + FProjectDescriptor::GetExtension()));
			return true;
		}
	}

#if WITH_EDITOR
	if (FEditorCommandLineUtils::ParseGameProjectPath(InCmdLine, OutProjectFilePath, OutGameName))
	{
		return true;
	}
#endif
	return false;
}

bool LaunchSetGameName(const TCHAR *InCmdLine)
{
	if (GIsGameAgnosticExe)
	{
		// Initialize GameName to an empty string. Populate it below.
		FApp::SetGameName(TEXT(""));

		FString ProjFilePath;
		FString LocalGameName;
		if (ParseGameProjectFromCommandLine(InCmdLine, ProjFilePath, LocalGameName) == true)
		{
			// Only set the game name if this is NOT a program...
			if (FPlatformProperties::IsProgram() == false)
			{
				FApp::SetGameName(*LocalGameName);
			}
			FPaths::SetProjectFilePath(ProjFilePath);
		}
#if UE_GAME
		else
		{
			// Try to use the executable name as the game name.
			LocalGameName = FPlatformProcess::ExecutableName();
			FApp::SetGameName(*LocalGameName);

			// Check it's not UE4Game, otherwise assume a uproject file relative to the game project directory
			if (LocalGameName != TEXT("UE4Game"))
			{
				ProjFilePath = FPaths::Combine(TEXT(".."), TEXT(".."), TEXT(".."), *LocalGameName, *FString(LocalGameName + TEXT(".") + FProjectDescriptor::GetExtension()));
				FPaths::SetProjectFilePath(ProjFilePath);
			}
		}
#endif

		static bool bPrinted = false;
		if (!bPrinted)
		{
			bPrinted = true;
			if (FApp::HasGameName())
			{
				UE_LOG(LogInit, Display, TEXT("Running engine for game: %s"), FApp::GetGameName());
			}
			else
			{
				if (FPlatformProperties::RequiresCookedData())
				{
					UE_LOG(LogInit, Fatal, TEXT("Non-agnostic games on cooked platforms require a uproject file be specified."));
				}
				else
				{
					UE_LOG(LogInit, Display, TEXT("Running engine without a game"));
				}
			}
		}
	}
	else
	{
		FString ProjFilePath;
		FString LocalGameName;
		if (ParseGameProjectFromCommandLine(InCmdLine, ProjFilePath, LocalGameName) == true)
		{
			if (FPlatformProperties::RequiresCookedData())
			{
				// Non-agnostic exes that require cooked data cannot load projects, so make sure that the LocalGameName is the GameName
				if (LocalGameName != FApp::GetGameName())
				{
					UE_LOG(LogInit, Fatal, TEXT("Non-agnostic games cannot load projects on cooked platforms - try running UE4Game."));
				}
			}
			// Only set the game name if this is NOT a program...
			if (FPlatformProperties::IsProgram() == false)
			{
				FApp::SetGameName(*LocalGameName);
			}
			FPaths::SetProjectFilePath(ProjFilePath);
		}

		// In a non-game agnostic exe, the game name should already be assigned by now.
		if (!FApp::HasGameName())
		{
			UE_LOG(LogInit, Fatal,TEXT("Could not set game name!"));
		}
	}

	return true;
}

void LaunchFixGameNameCase()
{
#if PLATFORM_DESKTOP && !IS_PROGRAM
	// This is to make sure this function is not misused and is only called when the game name is set
	check(FApp::HasGameName());

	// correct the case of the game name, if possible (unless we're running a program and the game name is already set)	
	if (FPaths::IsProjectFilePathSet())
	{
		const FString GameName(FPaths::GetBaseFilename(IFileManager::Get().GetFilenameOnDisk(*FPaths::GetProjectFilePath())));

		const bool bGameNameMatchesProjectCaseSensitive = (FCString::Strcmp(*GameName, FApp::GetGameName()) == 0);
		if (!bGameNameMatchesProjectCaseSensitive && (FApp::IsGameNameEmpty() || GIsGameAgnosticExe || (GameName.Len() > 0 && GIsGameAgnosticExe)))
		{
			if (GameName == FApp::GetGameName()) // case insensitive compare
			{
				FApp::SetGameName(*GameName);
			}
			else
			{
				const FText Message = FText::Format(
					NSLOCTEXT("Core", "MismatchedGameNames", "The name of the .uproject file ('{0}') must match the name of the project passed in the command line ('{1}')."),
					FText::FromString(*GameName),
					FText::FromString(FApp::GetGameName()));
				if (!GIsBuildMachine)
				{
					UE_LOG(LogInit, Warning, TEXT("%s"), *Message.ToString());
					FMessageDialog::Open(EAppMsgType::Ok, Message);
				}
				FApp::SetGameName(TEXT("")); // this disables part of the crash reporter to avoid writing log files to a bogus directory
				if (!GIsBuildMachine)
				{
					exit(1);
				}
				UE_LOG(LogInit, Fatal, TEXT("%s"), *Message.ToString());
			}
		}
	}
#endif	//PLATFORM_DESKTOP
}

static IPlatformFile* ConditionallyCreateFileWrapper(const TCHAR* Name, IPlatformFile* CurrentPlatformFile, const TCHAR* CommandLine, bool* OutFailedToInitialize = NULL, bool* bOutShouldBeUsed = NULL )
{
	if (OutFailedToInitialize)
	{
		*OutFailedToInitialize = false;
	}
	if ( bOutShouldBeUsed )
	{
		*bOutShouldBeUsed = false;
	}
	IPlatformFile* WrapperFile = FPlatformFileManager::Get().GetPlatformFile(Name);
	if (WrapperFile != NULL && WrapperFile->ShouldBeUsed(CurrentPlatformFile, CommandLine))
	{
		if ( bOutShouldBeUsed )
		{
			*bOutShouldBeUsed = true;
		}
		if (WrapperFile->Initialize(CurrentPlatformFile, CommandLine) == false)
		{
			if (OutFailedToInitialize)
			{
				*OutFailedToInitialize = true;
			}
			// Don't delete the platform file. It will be automatically deleted by its module.
			WrapperFile = NULL;
		}
	}
	else
	{
		// Make sure it won't be used.
		WrapperFile = NULL;
	}
	return WrapperFile;
}

/**
 * Look for any file overrides on the command line (i.e. network connection file handler)
 */
bool LaunchCheckForFileOverride(const TCHAR* CmdLine, bool& OutFileOverrideFound)
{
	OutFileOverrideFound = false;

	// Get the physical platform file.
	IPlatformFile* CurrentPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();

	// Try to create pak file wrapper
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("PakFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
		PlatformFile = ConditionallyCreateFileWrapper(TEXT("CachedReadFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}

	// Try to create sandbox wrapper
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("SandboxFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
	
#if !UE_BUILD_SHIPPING // UFS clients are not available in shipping builds.	
	// Streaming network wrapper (it has a priority over normal network wrapper)
	bool bNetworkFailedToInitialize = false;
	do
	{
		bool bShouldUseStreamingFile = false;
		IPlatformFile* NetworkPlatformFile = ConditionallyCreateFileWrapper(TEXT("StreamingFile"), CurrentPlatformFile, CmdLine, &bNetworkFailedToInitialize, &bShouldUseStreamingFile);
		if (NetworkPlatformFile)
		{
			CurrentPlatformFile = NetworkPlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}

		// if streaming network platform file was tried this loop don't try this one
		// Network file wrapper (only create if the streaming wrapper hasn't been created)
		if ( !bShouldUseStreamingFile && !NetworkPlatformFile)
		{
			NetworkPlatformFile = ConditionallyCreateFileWrapper(TEXT("NetworkFile"), CurrentPlatformFile, CmdLine, &bNetworkFailedToInitialize);
			if (NetworkPlatformFile)
			{
				CurrentPlatformFile = NetworkPlatformFile;
				FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
			}
		}
		

		if (bNetworkFailedToInitialize)
		{
			FString HostIpString;
			FParse::Value(CmdLine, TEXT("-FileHostIP="), HostIpString);
#if PLATFORM_REQUIRES_FILESERVER
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to connect to file server at %s. RETRYING.\n"), *HostIpString);
			uint32 Result = 2;
#else	//PLATFORM_REQUIRES_FILESERVER
			// note that this can't be localized because it happens before we connect to a filserver - localizing would cause ICU to try to load.... from over the file server connection!
			FString Error = FString::Printf(TEXT("Failed to connect to any of the following file servers:\n\n    %s\n\nWould you like to try again? No will fallback to local disk files, Cancel will quit."), *HostIpString.Replace( TEXT("+"), TEXT("\n    "))); 
			uint32 Result = FMessageDialog::Open( EAppMsgType::YesNoCancel, FText::FromString( Error ) );
#endif	//PLATFORM_REQUIRES_FILESERVER

			if (Result == EAppReturnType::No)
			{
				break;
			}
			else if (Result == EAppReturnType::Cancel)
			{
				// Cancel - return a failure, and quit
				return false;
			}
		}
	}
	while (bNetworkFailedToInitialize);
#endif

#if !UE_BUILD_SHIPPING
	// Try to create file profiling wrapper
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("ProfileFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("SimpleProfileFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
	// Try and create file timings stats wrapper
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("FileReadStats"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
	// Try and create file open log wrapper (lists the order files are first opened)
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("FileOpenLog"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}
#endif	//#if !UE_BUILD_SHIPPING

	// Wrap the above in a file logging singleton if requested
	{
		IPlatformFile* PlatformFile = ConditionallyCreateFileWrapper(TEXT("LogFile"), CurrentPlatformFile, CmdLine);
		if (PlatformFile)
		{
			CurrentPlatformFile = PlatformFile;
			FPlatformFileManager::Get().SetPlatformFile(*CurrentPlatformFile);
		}
	}

	// If our platform file is different than it was when we started, then an override was used
	OutFileOverrideFound = (CurrentPlatformFile != &FPlatformFileManager::Get().GetPlatformFile());

	return true;
}

bool LaunchHasIncompleteGameName()
{
	if ( FApp::HasGameName() && !FPaths::IsProjectFilePathSet() )
	{
		// Verify this is a legitimate game name
		// Launched with a game name. See if the <GameName> folder exists. If it doesn't, it could instead be <GameName>Game
		const FString NonSuffixedGameFolder = FPaths::RootDir() / FApp::GetGameName();
		if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*NonSuffixedGameFolder) == false)
		{
			const FString SuffixedGameFolder = NonSuffixedGameFolder + TEXT("Game");
			if (FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*SuffixedGameFolder))
			{
				return true;
			}
		}
	}

	return false;
}

void LaunchUpdateMostRecentProjectFile()
{
	// If we are launching without a game name or project file, we should use the last used project file, if it exists
	const FString& AutoLoadProjectFileName = IProjectManager::Get().GetAutoLoadProjectFileName();
	FString RecentProjectFileContents;
	if ( FFileHelper::LoadFileToString(RecentProjectFileContents, *AutoLoadProjectFileName) )
	{
		if ( RecentProjectFileContents.Len() )
		{
			const FString AutoLoadInProgressFilename = AutoLoadProjectFileName + TEXT(".InProgress");
			if ( FPlatformFileManager::Get().GetPlatformFile().FileExists(*AutoLoadInProgressFilename) )
			{
				// We attempted to auto-load a project but the last run did not make it to UEditorEngine::InitEditor.
				// This indicates that there was a problem loading the project.
				// Do not auto-load the project, instead load normally until the next time the editor starts successfully.
				UE_LOG(LogInit, Display, TEXT("There was a problem auto-loading %s. Auto-load will be disabled until the editor successfully starts up with a project."), *RecentProjectFileContents);
			}
			else if ( FPlatformFileManager::Get().GetPlatformFile().FileExists(*RecentProjectFileContents) )
			{
				// The previously loaded project file was found. Change the game name here and update the project file path
				FApp::SetGameName(*FPaths::GetBaseFilename(RecentProjectFileContents));
				FPaths::SetProjectFilePath(RecentProjectFileContents);
				UE_LOG(LogInit, Display, TEXT("Loading recent project file: %s"), *RecentProjectFileContents);

				// Write a file indicating that we are trying to auto-load a project.
				// This file prevents auto-loading of projects for as long as it exists. It is a detection system for failed auto-loads.
				// The file is deleted in UEditorEngine::InitEditor, thus if the load does not make it that far then the project will not be loaded again.
				FFileHelper::SaveStringToFile(TEXT(""), *AutoLoadInProgressFilename);
			}
		}
	}
}


/*-----------------------------------------------------------------------------
	FEngineLoop implementation.
-----------------------------------------------------------------------------*/

FEngineLoop::FEngineLoop()
#if WITH_ENGINE
	: EngineService(NULL)
#endif
{ }


int32 FEngineLoop::PreInit(int32 ArgC, TCHAR* ArgV[], const TCHAR* AdditionalCommandline)
{
	FString CmdLine;

	// loop over the parameters, skipping the first one (which is the executable name)
	for (int32 Arg = 1; Arg < ArgC; Arg++)
	{
		FString ThisArg = ArgV[Arg];
		if (ThisArg.Contains(TEXT(" ")) && !ThisArg.Contains(TEXT("\"")))
		{
			int32 EqualsAt = ThisArg.Find(TEXT("="));
			if (EqualsAt > 0 && ThisArg.Find(TEXT(" ")) > EqualsAt)
			{
				ThisArg = ThisArg.Left(EqualsAt + 1) + FString("\"") + ThisArg.RightChop(EqualsAt + 1) + FString("\"");

			}
			else
			{
				ThisArg = FString("\"") + ThisArg + FString("\"");
			}
		}

		CmdLine += ThisArg;
		// put a space between each argument (not needed after the end)
		if (Arg + 1 < ArgC)
		{
			CmdLine += TEXT(" ");
		}
	}

	// append the additional extra command line
	if (AdditionalCommandline)
	{
		CmdLine += TEXT(" ");
		CmdLine += AdditionalCommandline;
	}

	// send the command line without the exe name
	return GEngineLoop.PreInit(*CmdLine);
}

#if WITH_ENGINE
bool IsServerDelegateForOSS(FName WorldContextHandle)
{
	if (IsRunningDedicatedServer())
	{
		return true;
	}

	UWorld* World = NULL;
	if (WorldContextHandle != NAME_None)
	{
		FWorldContext& WorldContext = GEngine->GetWorldContextFromHandleChecked(WorldContextHandle);
		check(WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE);
		World = WorldContext.World();
	}
	else
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

		if (GameEngine)
		{
			World = GameEngine->GetGameWorld();
		}
		else
		{
			UE_LOG(LogInit, Error, TEXT("Failed to determine if OSS is server in PIE, OSS requests will fail"));
			return false;
		}
	}

	ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	return (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer);
}
#endif

int32 FEngineLoop::PreInit( const TCHAR* CmdLine )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Engine Pre-Initialized"), STAT_PreInit, STATGROUP_LoadTime);

	if (FParse::Param(CmdLine, TEXT("UTF8Output")))
	{
		FPlatformMisc::SetUTF8Output();
	}

	// Switch into executable's directory.
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	// this is set later with shorter command lines, but we want to make sure it is set ASAP as some subsystems will do the tests themselves...
	// also realize that command lines can be pulled from the network at a slightly later time.
	if (!FCommandLine::Set(CmdLine))
	{
		// Fail, shipping builds will crash if setting command line fails
		return -1;
	}

#if	STATS
	// Create the stats malloc profiler proxy.
	if( FStatsMallocProfilerProxy::HasMemoryProfilerToken() )
	{
		// Assumes no concurrency here.
		GMalloc = FStatsMallocProfilerProxy::Get();
	}
#endif // STATS

	// Set GameName, based on the command line
	if (LaunchSetGameName(CmdLine) == false)
	{
		// If it failed, do not continue
		return 1;
	}

	// Initialize log console here to avoid statics initialization issues when launched from the command line.
	GScopedLogConsole = FPlatformOutputDevices::GetLogConsole();

	// Always enable the backlog so we get all messages, we will disable and clear it in the game
	// as soon as we determine whether GIsEditor == false
	GLog->EnableBacklog(true);

	// Initialize std out device as early as possible if requested in the command line
	if (FParse::Param(FCommandLine::Get(), TEXT("stdout")))
	{
		InitializeStdOutDevice();
	}

#if !UE_BUILD_SHIPPING
	if (FPlatformProperties::SupportsQuit())
	{
		FString ExitPhrases;
		if (FParse::Value(FCommandLine::Get(), TEXT("testexit="), ExitPhrases))
		{
			TArray<FString> ExitPhrasesList;
			if (ExitPhrases.ParseIntoArray(ExitPhrasesList, TEXT("+"), true) > 0)
			{
				GScopedTestExit = new FOutputDeviceTestExit(ExitPhrasesList);
				GLog->AddOutputDevice(GScopedTestExit.GetOwnedPointer());
			}
		}
	}
#endif // !UE_BUILD_SHIPPING

	// Switch into executable's directory (may be required by some of the platform file overrides)
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	// This fixes up the relative project path, needs to happen before we set platform file paths
	if (FPlatformProperties::IsProgram() == false)
	{
		if (FPaths::IsProjectFilePathSet())
		{
			FString ProjPath = FPaths::GetProjectFilePath();
			if (FPaths::FileExists(ProjPath) == false)
			{
				UE_LOG(LogInit, Display, TEXT("Project file not found: %s"), *ProjPath);
				UE_LOG(LogInit, Display, TEXT("\tAttempting to find via project info helper."));
				// Use the uprojectdirs
				FString GameProjectFile = FUProjectDictionary::GetDefault().GetRelativeProjectPathForGame(FApp::GetGameName(), FPlatformProcess::BaseDir());
				if (GameProjectFile.IsEmpty() == false)
				{
					UE_LOG(LogInit, Display, TEXT("\tFound project file %s."), *GameProjectFile);
					FPaths::SetProjectFilePath(GameProjectFile);
				}
			}
		}
	}

	// allow the command line to override the platform file singleton
	bool bFileOverrideFound = false;
	if (LaunchCheckForFileOverride(CmdLine, bFileOverrideFound) == false)
	{
		// if it failed, we cannot continue
		return 1;
	}

	// Initialize file manager
	IFileManager::Get().ProcessCommandLineOptions();

	if( GIsGameAgnosticExe )
	{
		// If we launched without a project file, but with a game name that is incomplete, warn about the improper use of a Game suffix
		if ( LaunchHasIncompleteGameName() )
		{
			// We did not find a non-suffixed folder and we DID find the suffixed one.
			// The engine MUST be launched with <GameName>Game.
			const FText GameNameText = FText::FromString(FApp::GetGameName());
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format( LOCTEXT("RequiresGamePrefix", "Error: UE4Editor does not append 'Game' to the passed in game name.\nYou must use the full name.\nYou specified '{0}', use '{0}Game'."), GameNameText ) );
			return 1;
		}
	}

	// remember thread id of the main thread
	GGameThreadId = FPlatformTLS::GetCurrentThreadId();
	GIsGameThreadIdInitialized = true;

	FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetMainGameMask());
	FPlatformProcess::SetupGameOrRenderThread(false);

	// Figure out whether we're the editor, ucc or the game.
	const SIZE_T CommandLineSize = FCString::Strlen(CmdLine)+1;
	TCHAR* CommandLineCopy			= new TCHAR[ CommandLineSize ];
	FCString::Strcpy( CommandLineCopy, CommandLineSize, CmdLine );
	const TCHAR* ParsedCmdLine	= CommandLineCopy;

	FString Token				= FParse::Token( ParsedCmdLine, 0);

#if WITH_ENGINE
	TArray<FString> Tokens;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine(CommandLineCopy, Tokens, Switches);

	bool bHasCommandletToken = false;

	for( int32 TokenIndex = 0; TokenIndex < Tokens.Num(); ++TokenIndex )
	{
		if( Tokens[TokenIndex].EndsWith(TEXT("Commandlet")) )
		{
			bHasCommandletToken = true;
			Token = Tokens[TokenIndex];
			break;
		}
	}

	for( int32 SwitchIndex = 0; SwitchIndex < Switches.Num() && !bHasCommandletToken; ++SwitchIndex )
	{
		if( Switches[SwitchIndex].StartsWith(TEXT("RUN=")) )
		{
			bHasCommandletToken = true;
			Token = Switches[SwitchIndex];
			break;
		}
	}

#endif // UE_EDITOR


	// trim any whitespace at edges of string - this can happen if the token was quoted with leading or trailing whitespace
	// VC++ tends to do this in its "external tools" config
	Token = Token.Trim();

	// Path returned by FPaths::GetProjectFilePath() is normalized, so may have symlinks and ~ resolved and may differ from the original path to .uproject passed in the command line
	FString NormalizedToken = Token;
	FPaths::NormalizeFilename(NormalizedToken);

	const bool bFirstTokenIsGameName = (FApp::HasGameName() && Token == FApp::GetGameName());
	const bool bFirstTokenIsGameProjectFilePath = (FPaths::IsProjectFilePathSet() && NormalizedToken == FPaths::GetProjectFilePath());
	const bool bFirstTokenIsGameProjectFileShortName = (FPaths::IsProjectFilePathSet() && Token == FPaths::GetCleanFilename(FPaths::GetProjectFilePath()));

	if (bFirstTokenIsGameName || bFirstTokenIsGameProjectFilePath || bFirstTokenIsGameProjectFileShortName)
	{
		// first item on command line was the game name, remove it in all cases
		FString RemainingCommandline = ParsedCmdLine;
		FCString::Strcpy( CommandLineCopy, CommandLineSize, *RemainingCommandline );
		ParsedCmdLine	= CommandLineCopy;

		// Set a new command-line that doesn't include the game name as the first argument
		FCommandLine::Set(ParsedCmdLine); 

		Token = FParse::Token( ParsedCmdLine, 0);
		Token = Token.Trim();

		// if the next token is a project file, then we skip it (which can happen on some platforms that combine
		// commandlines... this handles extra .uprojects, but if you run with MyGame MyGame, we can't tell if
		// the second MyGame is a map or not)
		while (FPaths::GetExtension(Token) == FProjectDescriptor::GetExtension())
		{
			Token = FParse::Token(ParsedCmdLine, 0);
			Token = Token.Trim();
		}

		if (bFirstTokenIsGameProjectFilePath || bFirstTokenIsGameProjectFileShortName)
		{
			// Convert it to relative if possible...
			FString RelativeGameProjectFilePath = FFileManagerGeneric::DefaultConvertToRelativePath(*FPaths::GetProjectFilePath());
			if (RelativeGameProjectFilePath != FPaths::GetProjectFilePath())
			{
				FPaths::SetProjectFilePath(RelativeGameProjectFilePath);
			}
		}
	}

	// look early for the editor token
	bool bHasEditorToken = false;

#if UE_EDITOR
	// Check each token for '-game', '-server' or '-run='
	bool bIsNotEditor = false;

	// This isn't necessarily pretty, but many requests have been made to allow
	//   UE4Editor.exe <GAMENAME> -game <map>
	// or
	//   UE4Editor.exe <GAMENAME> -game 127.0.0.0
	// We don't want to remove the -game from the commandline just yet in case 
	// we need it for something later. So, just move it to the end for now...
	const bool bFirstTokenIsGame = (Token == TEXT("-GAME"));
	const bool bFirstTokenIsServer = (Token == TEXT("-SERVER"));
	const bool bFirstTokenIsModeOverride = bFirstTokenIsGame || bFirstTokenIsServer || bHasCommandletToken;
	const TCHAR* CommandletCommandLine = NULL;
	if (bFirstTokenIsModeOverride)
	{
		bIsNotEditor = true;
		if (bFirstTokenIsGame || bFirstTokenIsServer)
		{
			// Move the token to the end of the list...
			FString RemainingCommandline = ParsedCmdLine;
			RemainingCommandline = RemainingCommandline.Trim();
			RemainingCommandline += FString::Printf(TEXT(" %s"), *Token);
			FCommandLine::Set(*RemainingCommandline); 
		}
		if (bHasCommandletToken)
		{
#if STATS
			FThreadStats::MasterDisableForever();
#endif
			if (Token.StartsWith(TEXT("run=")))
			{
				Token = Token.RightChop(4);
				if (!Token.EndsWith(TEXT("Commandlet")))
				{
					Token += TEXT("Commandlet");
				}
			}
			CommandletCommandLine = ParsedCmdLine;
		}

	}

	if( !bIsNotEditor && GIsGameAgnosticExe )
	{
		// If we launched without a game name or project name, try to load the most recently loaded project file.
		// We can not do this if we are using a FilePlatform override since the game directory may already be established.
		const bool bIsBuildMachine = FParse::Param(FCommandLine::Get(), TEXT("BUILDMACHINE"));
		const bool bLoadMostRecentProjectFileIfItExists = !FApp::HasGameName() && !bFileOverrideFound && !bIsBuildMachine && !FParse::Param( CmdLine, TEXT("norecentproject") );
		if (bLoadMostRecentProjectFileIfItExists )
		{
			LaunchUpdateMostRecentProjectFile();
		}
	}

#if !UE_BUILD_SHIPPING
	// Benchmarking.
	FApp::SetBenchmarking(FParse::Param(FCommandLine::Get(),TEXT("BENCHMARK")));
#else
	FApp::SetBenchmarking(false);
#endif // !UE_BUILD_SHIPPING

	// Initialize random number generator.
	if( FApp::IsBenchmarking() || FParse::Param(FCommandLine::Get(),TEXT("FIXEDSEED")) )
	{
		FMath::RandInit( 0 );
		FMath::SRandInit( 0 );
		UE_LOG(LogInit, Display, TEXT("RandInit(0) SRandInit(0)."));
	}
	else
	{
		uint32 Cycles1 = FPlatformTime::Cycles();
		FMath::RandInit(Cycles1);
		uint32 Cycles2 = FPlatformTime::Cycles();
		FMath::SRandInit(Cycles2);
		UE_LOG(LogInit, Display, TEXT("RandInit(%d) SRandInit(%d)."), Cycles1, Cycles2);
	}


	FString CheckToken = Token;
	bool bFoundValidToken = false;
	while (!bFoundValidToken && (CheckToken.Len() > 0))
	{
		if (!bIsNotEditor)
		{
			bool bHasNonEditorToken = (CheckToken == TEXT("-GAME")) || (CheckToken == TEXT("-SERVER")) || (CheckToken.StartsWith(TEXT("RUN="))) || CheckToken.EndsWith(TEXT("Commandlet"));
			if (bHasNonEditorToken)
			{
				bIsNotEditor = true;
				bFoundValidToken = true;
			}
		}
		
		CheckToken = FParse::Token(ParsedCmdLine, 0);
	}

	bHasEditorToken = !bIsNotEditor;
#elif WITH_ENGINE
	const TCHAR* CommandletCommandLine = NULL;
	if (bHasCommandletToken)
	{
#if STATS
		FThreadStats::MasterDisableForever();
#endif
		if (Token.StartsWith(TEXT("run=")))
		{
			Token = Token.RightChop(4);
			if (!Token.EndsWith(TEXT("Commandlet")))
			{
				Token += TEXT("Commandlet");
			}
		}
		CommandletCommandLine = ParsedCmdLine;
	}
#if WITH_EDITOR && WITH_EDITORONLY_DATA
	// If a non-editor target build w/ WITH_EDITOR and WITH_EDITORONLY_DATA, use the old token check...
	//@todo. Is this something we need to support?
	bHasEditorToken = Token == TEXT("EDITOR");
#else
	// Game, server and commandlets never set the editor token
	bHasEditorToken = false;
#endif
#endif	//UE_EDITOR

#if !IS_PROGRAM
	if ( !GIsGameAgnosticExe && FApp::HasGameName() && !FPaths::IsProjectFilePathSet() )
	{
		// If we are using a non-agnostic exe where a name was specified but we did not specify a project path. Assemble one based on the game name.
		const FString ProjectFilePath = FPaths::Combine(*FPaths::GameDir(), *FString::Printf(TEXT("%s.%s"), FApp::GetGameName(), *FProjectDescriptor::GetExtension()));
		FPaths::SetProjectFilePath(ProjectFilePath);
	}

	// Now verify the project file if we have one
	if ( FPaths::IsProjectFilePathSet() )
	{
		if ( !IProjectManager::Get().LoadProjectFile(FPaths::GetProjectFilePath()) )
		{
			// The project file was invalid or saved with a newer version of the engine. Exit.
			UE_LOG(LogInit, Warning, TEXT("Could not find a valid project file, the engine will exit now."));
			return 1;
		}
	}

	if( FApp::HasGameName() )
	{
		// Tell the module manager what the game binaries folder is
		const FString GameBinariesDirectory = FPaths::Combine( FPlatformMisc::GameDir(), TEXT( "Binaries" ), FPlatformProcess::GetBinariesSubdirectory() );
		FModuleManager::Get().SetGameBinariesDirectory(*GameBinariesDirectory);

		LaunchFixGameNameCase();
	}
#endif

	// Load Core modules required for everything else to work (needs to be loaded before InitializeRenderingCVarsCaching)
	LoadCoreModules();

#if WITH_ENGINE
	extern ENGINE_API void InitializeRenderingCVarsCaching();
	InitializeRenderingCVarsCaching();
#endif

	bool bTokenDoesNotHaveDash = Token.Len() && FCString::Strnicmp(*Token, TEXT("-"), 1) != 0;

#if WITH_EDITOR
	// If we're running as an game but don't have a project, inform the user and exit.
	if (bHasEditorToken == false && bHasCommandletToken == false)
	{
		if ( !FPaths::IsProjectFilePathSet() )
		{
			//@todo this is too early to localize
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Engine", "UE4RequiresProjectFiles", "UE4 games require a project file as the first parameter."));
			return 1;
		}
	}

	if (GIsUCCMakeStandaloneHeaderGenerator)
	{
		// Rebuilding script requires some hacks in the engine so we flag that.
		PRIVATE_GIsRunningCommandlet = true;
	}
#endif //WITH_EDITOR

	// initialize task graph sub-system with potential multiple threads
	FTaskGraphInterface::Startup( FPlatformMisc::NumberOfCores() );
	FTaskGraphInterface::Get().AttachToThread( ENamedThreads::GameThread );

#if STATS
	FThreadStats::StartThread();
#endif

	if (FPlatformProcess::SupportsMultithreading())
	{
		GThreadPool	= FQueuedThreadPool::Allocate();
		int32 NumThreadsInThreadPool = FPlatformMisc::NumberOfWorkerThreadsToSpawn();

		// we are only going to give dedicated servers one pool thread
		if (FPlatformProperties::IsServerOnly())
		{
			NumThreadsInThreadPool = 1;
		}
		verify(GThreadPool->Create(NumThreadsInThreadPool));
	}

	// Get a pointer to the log output device
	GLogConsole = GScopedLogConsole.GetOwnedPointer();

	LoadPreInitModules();

	// Start the application
	if(!AppInit())
	{
		return 1;
	}
	
#if WITH_ENGINE
	// Initialize system settings before anyone tries to use it...
	GSystemSettings.Initialize( bHasEditorToken );

	// Apply renderer settings from console variables stored in the INI.
	ApplyCVarSettingsFromIni(TEXT("/Script/Engine.RendererSettings"),*GEngineIni, ECVF_SetByProjectSetting);

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (!IsRunningCommandlet())
		{
			// Note: It is critical that resolution settings are loaded before the movie starts playing so that the window size and fullscreen state is known
			UGameUserSettings::PreloadResolutionSettings();
		}
	}
#endif

	// As early as possible to avoid expensive re-init of subsystems, 
	// after SystemSettings.ini file loading so we get the right state,
	// before ConsoleVariables.ini so the local developer can always override.
	// before InitializeCVarsForActiveDeviceProfile() so the platform can override user settings
	Scalability::LoadState((bHasEditorToken && !GEditorSettingsIni.IsEmpty()) ? GEditorSettingsIni : GGameUserSettingsIni);

	// Set all CVars which have been setup in the device profiles.
	UDeviceProfileManager::InitializeCVarsForActiveDeviceProfile();

	if (FApp::ShouldUseThreadingForPerformance() && FPlatformMisc::AllowRenderThread())
	{
		GUseThreadedRendering = true;
	}
#endif

	FConfigCacheIni::LoadConsoleVariablesFromINI();

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Platform Initialization"), STAT_PlatformInit, STATGROUP_LoadTime);

		// platform specific initialization now that the SystemSettings are loaded
		FPlatformMisc::PlatformInit();
		FPlatformMemory::Init();
	}

	// Let LogConsole know what ini file it should use to save its setting on exit.
	// We can't use GGameIni inside log console because it's destroyed in the global
	// scoped pointer and at that moment GGameIni may already be gone.
	if( GLogConsole != NULL )
	{
		GLogConsole->SetIniFilename(*GGameIni);
	}


#if CHECK_PUREVIRTUALS
	FMessageDialog::Open( EAppMsgType::Ok, *NSLOCTEXT("Engine", "Error_PureVirtualsEnabled", "The game cannot run with CHECK_PUREVIRTUALS enabled.  Please disable CHECK_PUREVIRTUALS and rebuild the executable.").ToString() );
	FPlatformMisc::RequestExit(false);
#endif

#if WITH_ENGINE
	// allow for game explorer processing (including parental controls) and firewalls installation
	if (!FPlatformMisc::CommandLineCommands())
	{
		FPlatformMisc::RequestExit(false);
	}
	
	bool bIsSeekFreeDedicatedServer = false;	
	bool bIsRegularClient = false;

	if (!bHasEditorToken)
	{
		// See whether the first token on the command line is a commandlet.

		//@hack: We need to set these before calling StaticLoadClass so all required data gets loaded for the commandlets.
		GIsClient = true;
		GIsServer = true;
#if WITH_EDITOR
		GIsEditor = true;
#endif	//WITH_EDITOR
		PRIVATE_GIsRunningCommandlet = true;

		// We need to disregard the empty token as we try finding Token + "Commandlet" which would result in finding the
		// UCommandlet class if Token is empty.
		bool bDefinitelyCommandlet = (bTokenDoesNotHaveDash && Token.EndsWith(TEXT("Commandlet")));
		if (!bTokenDoesNotHaveDash)
		{
			if (Token.StartsWith(TEXT("run=")))
			{
				Token = Token.RightChop(4);
				bDefinitelyCommandlet = true;
				if (!Token.EndsWith(TEXT("Commandlet")))
				{
					Token += TEXT("Commandlet");
				}
			}
		}
		else 
		{
			if (!bDefinitelyCommandlet)
			{
				UClass* TempCommandletClass = FindObject<UClass>(ANY_PACKAGE, *(Token+TEXT("Commandlet")), false);

				if (TempCommandletClass)
				{
					check(TempCommandletClass->IsChildOf(UCommandlet::StaticClass())); // ok so you have a class that ends with commandlet that is not a commandlet
					
					Token += TEXT("Commandlet");
					bDefinitelyCommandlet = true;
				}
			}
		}

		if (!bDefinitelyCommandlet)
		{
			bIsRegularClient = true;
			GIsClient = true;
			GIsServer = false;
#if WITH_EDITORONLY_DATA
			GIsEditor = false;
#endif
			PRIVATE_GIsRunningCommandlet = false;
		}
	}

	if (IsRunningDedicatedServer())
	{
		GIsClient = false;
		GIsServer = true;
		PRIVATE_GIsRunningCommandlet = false;
#if WITH_EDITOR
		GIsEditor = false;
#endif
		bIsSeekFreeDedicatedServer = FPlatformProperties::RequiresCookedData();
	}

	// If std out device hasn't been initialized yet (there was no -stdout param in the command line) and
	// we meet all the criteria, initialize it now.
	if (!GScopedStdOut.IsValid() && !bHasEditorToken && !bIsRegularClient && !IsRunningDedicatedServer())
	{
		InitializeStdOutDevice();
	}

	FIOSystem::Get(); // force it to be created if it isn't already

	// allow the platform to start up any features it may need
	IPlatformFeaturesModule::Get();

	// Init physics engine before loading anything, in case we want to do things like cook during post-load.
	InitGamePhys();

	// Delete temporary files in cache.
	FPlatformProcess::CleanFileCache();

#if !UE_BUILD_SHIPPING
	GIsDemoMode = FParse::Param( FCommandLine::Get(), TEXT( "DEMOMODE" ) );
#endif

	if (bHasEditorToken)
	{	
#if WITH_EDITOR

		// We're the editor.
		GIsClient = true;
		GIsServer = true;
		GIsEditor = true;
		PRIVATE_GIsRunningCommandlet = false;

		GWarn = &UnrealEdWarn;

#else	
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("Engine", "EditorNotSupported", "Editor not supported in this mode."));
		FPlatformMisc::RequestExit(false);
		return 1;
#endif //WITH_EDITOR
	}

#endif // WITH_ENGINE
	// If we're not in the editor stop collecting the backlog now that we know
	if (!GIsEditor)
	{
		GLog->EnableBacklog( false );
	}
#if WITH_ENGINE

	EndInitTextLocalization();

	IStreamingManager::Get();

	if (FPlatformProcess::SupportsMultithreading() && !IsRunningDedicatedServer() && (bIsRegularClient || bHasEditorToken))
	{
		FPlatformSplash::Show();
	}

	if (!IsRunningDedicatedServer() && (bHasEditorToken || bIsRegularClient))
	{
		// Init platform application
		FSlateApplication::Create();
	}
	else
	{
		// If we're not creating the slate application there is some basic initialization
		// that it does that still must be done
		EKeys::Initialize();
		FCoreStyle::ResetToDefault();
	}

#if !UE_SERVER
	FEngineFontServices::Create();
#endif
	
	FScopedSlowTask SlowTask(100, NSLOCTEXT("EngineLoop", "EngineLoop_Initializing", "Initializing..."));

	SlowTask.EnterProgressFrame(10);

	// Initialize the RHI.
	RHIInit(bHasEditorToken);

	if (!FPlatformProperties::RequiresCookedData())
	{
		check(!GShaderCompilingManager);
		GShaderCompilingManager = new FShaderCompilingManager();

		check(!GDistanceFieldAsyncQueue);
		GDistanceFieldAsyncQueue = new FDistanceFieldAsyncQueue();
	}

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Initial UObject load"), STAT_InitialUObjectLoad, STATGROUP_LoadTime);

		// Initialize shader types before loading any shaders
		InitializeShaderTypes();

		SlowTask.EnterProgressFrame(30);
	
		// Load the global shaders.
		if (GetGlobalShaderMap(GMaxRHIFeatureLevel) == NULL && GIsRequestingExit)
		{
			// This means we can't continue without the global shader map.
			return 1;
		}

		// In order to be able to use short script package names get all script
		// package names from ini files and register them with FPackageName system.
		FPackageName::RegisterShortPackageNamesForUObjectModules();

		SlowTask.EnterProgressFrame(5);

		// Make sure all UObject classes are registered and default properties have been initialized
		ProcessNewlyLoadedUObjects();

		// Default materials may have been loaded due to dependencies when loading
		// classes and class default objects. If not, do so now.
		UMaterialInterface::InitDefaultMaterials();
		UMaterialInterface::AssertDefaultMaterialsExist();
		UMaterialInterface::AssertDefaultMaterialsPostLoaded();
	}

	SlowTask.EnterProgressFrame(5);

	// Tell the module manager is may now process newly-loaded UObjects when new C++ modules are loaded
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// load up the seek-free startup packages
	if ( !FStartupPackages::LoadAll() )
	{
		// At least one startup package failed to load, return 1 to indicate an error
		return 1;
	}

	// Setup GC optimizations
	if (bIsSeekFreeDedicatedServer || bHasEditorToken)
	{
		GetUObjectArray().DisableDisregardForGC();
	}
	
	SlowTask.EnterProgressFrame(10);

	if ( !LoadStartupCoreModules() )
	{
		// At least one startup module failed to load, return 1 to indicate an error
		return 1;
	}

#if !UE_SERVER// && !UE_EDITOR
	if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		TSharedRef<FSlateRenderer> SlateRenderer = FModuleManager::Get().GetModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer();

		// If Slate is being used, initialize the renderer after RHIInit 
		FSlateApplication& CurrentSlateApp = FSlateApplication::Get();
		CurrentSlateApp.InitializeRenderer( SlateRenderer );

		GetMoviePlayer()->SetSlateRenderer(SlateRenderer);
	}
#endif

	SlowTask.EnterProgressFrame(10);
	
	// Load up all modules that need to hook into the loading screen
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PreLoadingScreen) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreLoadingScreen))
	{
		return 1;
	}

#if !UE_SERVER
	if ( !IsRunningDedicatedServer() )
	{
		// @todo ps4: If a loading movie starts earlier, which it probably should, then please see PS4's PlatformPostInit() implementation!

		// allow the movie player to load a sequence from the .inis (a PreLoadingScreen module could have already initialized a sequence, in which case
		// it wouldn't have anything in it's .ini file)
		GetMoviePlayer()->SetupLoadingScreenFromIni();

		GetMoviePlayer()->Initialize();
		GetMoviePlayer()->PlayMovie();

		// do any post appInit processing, before the render thread is started.
		FPlatformMisc::PlatformPostInit(!GetMoviePlayer()->IsMovieCurrentlyPlaying());
	}
	else
#endif
	{
		// do any post appInit processing, before the render thread is started.
		FPlatformMisc::PlatformPostInit(true);
	}
	SlowTask.EnterProgressFrame(5);

	if (GUseThreadedRendering)
	{
		if (GRHISupportsRHIThread)
		{
			const bool DefaultUseRHIThread = true;
			GUseRHIThread = DefaultUseRHIThread;
			if (FParse::Param(FCommandLine::Get(),TEXT("rhithread")))
			{
				GUseRHIThread = true;
			}
			else if (FParse::Param(FCommandLine::Get(),TEXT("norhithread")))
			{
				GUseRHIThread = false;
			}
		}
		StartRenderingThread();
	}
	
	if ( !LoadStartupModules() )
	{
		// At least one startup module failed to load, return 1 to indicate an error
		return 1;
	}

	MarkObjectsToDisregardForGC(); 
	GetUObjectArray().CloseDisregardForGC();

	SetIsServerForOnlineSubsystemsDelegate(FQueryIsRunningServer::CreateStatic(&IsServerDelegateForOSS));

	SlowTask.EnterProgressFrame(5);

	if (!bHasEditorToken)
	{
		UClass* CommandletClass = NULL;

		if (!bIsRegularClient)
		{
			CommandletClass = FindObject<UClass>(ANY_PACKAGE,*Token,false);
			if (!CommandletClass)
			{
				if (GLogConsole && !GIsSilent)
				{
					GLogConsole->Show(true);
				}
				UE_LOG(LogInit, Error, TEXT("%s looked like a commandlet, but we could not find the class."), *Token);
				GIsRequestingExit = true;
				return 1;
			}

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
			extern bool GIsConsoleExecutable;
			if (GIsConsoleExecutable)
			{
				if (GLogConsole != NULL && GLogConsole->IsAttached())
				{
					GLog->RemoveOutputDevice(GLogConsole);
				}
				// Setup Ctrl-C handler for console application
				FPlatformMisc::SetGracefulTerminationHandler();
			}
			else
#endif
			{
				// Bring up console unless we're a silent build.
				if( GLogConsole && !GIsSilent )
				{
					GLogConsole->Show( true );
				}
			}

			// print output immediately
			setvbuf(stdout, NULL, _IONBF, 0);

			UE_LOG(LogInit, Log,  TEXT("Executing %s"), *CommandletClass->GetFullName() );

			// Allow commandlets to individually override those settings.
			UCommandlet* Default = CastChecked<UCommandlet>(CommandletClass->GetDefaultObject());

			if ( GIsRequestingExit )
			{
				// commandlet set GIsRequestingExit during construction
				return 1;
			}

			GIsClient = Default->IsClient;
			GIsServer = Default->IsServer;
#if WITH_EDITOR
			GIsEditor = Default->IsEditor;
#else
			if (Default->IsEditor)
			{
				UE_LOG(LogInit, Error, TEXT("Cannot run editor commandlet %s with game executable."), *CommandletClass->GetFullName());
				GIsRequestingExit = true;
				return 1;
			}
#endif
			PRIVATE_GIsRunningCommandlet = true;
			// Reset aux log if we don't want to log to the console window.
			if( !Default->LogToConsole )
			{
				GLog->RemoveOutputDevice( GLogConsole );
			}

			GIsRequestingExit = true;	// so CTRL-C will exit immediately
			
			// allow the commandlet the opportunity to create a custom engine
			CommandletClass->GetDefaultObject<UCommandlet>()->CreateCustomEngine(CommandletCommandLine);
			if ( GEngine == NULL )
			{
#if WITH_EDITOR
				if ( GIsEditor )
				{
					UClass* EditorEngineClass = StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:/Script/Engine.Engine.EditorEngine"), NULL, LOAD_None, NULL );

					GEngine = GEditor = NewObject<UEditorEngine>(GetTransientPackage(), EditorEngineClass);

					GEngine->ParseCommandline();

					UE_LOG(LogInit, Log, TEXT("Initializing Editor Engine..."));
					GEditor->InitEditor(this);
					UE_LOG(LogInit, Log, TEXT("Initializing Editor Engine Completed"));
				}
				else
#endif
				{
					UClass* EngineClass = StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("engine-ini:/Script/Engine.Engine.GameEngine"), NULL, LOAD_None, NULL );

					// must do this here so that the engine object that we create on the next line receives the correct property values
					GEngine = NewObject<UEngine>(GetTransientPackage(), EngineClass);
					check(GEngine);

					GEngine->ParseCommandline();

					UE_LOG(LogInit, Log, TEXT("Initializing Game Engine..."));
					GEngine->Init(this);
					UE_LOG(LogInit, Log, TEXT("Initializing Game Engine Completed"));
				}
			}

			//run automation smoke tests now that the commandlet has had a chance to override the above flags and GEngine is available
#if !PLATFORM_HTML5 && !PLATFORM_HTML5_WIN32 
			FAutomationTestFramework::GetInstance().RunSmokeTests();
#endif 
	
			UCommandlet* Commandlet = NewObject<UCommandlet>(GetTransientPackage(), CommandletClass);
			check(Commandlet);
			Commandlet->AddToRoot();			

			// Execute the commandlet.
			double CommandletExecutionStartTime = FPlatformTime::Seconds();

			// Commandlets don't always handle -run= properly in the commandline so we'll provide them
			// with a custom version that doesn't have it.
			Commandlet->ParseParms( CommandletCommandLine );
			int32 ErrorLevel = Commandlet->Main( CommandletCommandLine );

			// Log warning/ error summary.
			if( Commandlet->ShowErrorCount )
			{
				if( GWarn->Errors.Num() || GWarn->Warnings.Num() )
				{
					SET_WARN_COLOR(COLOR_WHITE);
					UE_LOG(LogInit, Display, TEXT(""));
					UE_LOG(LogInit, Display, TEXT("Warning/Error Summary"));
					UE_LOG(LogInit, Display, TEXT("---------------------"));

					static const int32 MaxMessagesToShow = 50;
					TSet<FString> ShownMessages;
					
					SET_WARN_COLOR(COLOR_RED);
					ShownMessages.Empty(MaxMessagesToShow);
					for(auto It = GWarn->Errors.CreateConstIterator(); It; ++It)
					{
						bool bAlreadyShown = false;
						ShownMessages.Add(*It, &bAlreadyShown);

						if (!bAlreadyShown)
						{
							if (ShownMessages.Num() > MaxMessagesToShow)
							{
								SET_WARN_COLOR(COLOR_WHITE);
								UE_LOG(LogInit, Display, TEXT("NOTE: Only first %d errors displayed."), MaxMessagesToShow);
								break;
							}

							UE_LOG(LogInit, Display, TEXT("%s"), **It);
						}
					}

					SET_WARN_COLOR(COLOR_YELLOW);
					ShownMessages.Empty(MaxMessagesToShow);
					for(auto It = GWarn->Warnings.CreateConstIterator(); It; ++It)
					{
						bool bAlreadyShown = false;
						ShownMessages.Add(*It, &bAlreadyShown);

						if (!bAlreadyShown)
						{
							if (ShownMessages.Num() > MaxMessagesToShow)
							{
								SET_WARN_COLOR(COLOR_WHITE);
								UE_LOG(LogInit, Display, TEXT("NOTE: Only first %d warnings displayed."), MaxMessagesToShow);
								break;
							}

							UE_LOG(LogInit, Display, TEXT("%s"), **It);
						}
					}
				}

				UE_LOG(LogInit, Display, TEXT(""));

				if( ErrorLevel != 0 )
				{
					SET_WARN_COLOR(COLOR_RED);
					UE_LOG(LogInit, Display, TEXT("Commandlet->Main return this error code: %d"), ErrorLevel );
					UE_LOG(LogInit, Display, TEXT("With %d error(s), %d warning(s)"), GWarn->Errors.Num(), GWarn->Warnings.Num() );
				}
				else if( ( GWarn->Errors.Num() == 0 ) )
				{
					SET_WARN_COLOR(GWarn->Warnings.Num() ? COLOR_YELLOW : COLOR_GREEN);
					UE_LOG(LogInit, Display, TEXT("Success - %d error(s), %d warning(s)"), GWarn->Errors.Num(), GWarn->Warnings.Num() );
				}
				else
				{
					SET_WARN_COLOR(COLOR_RED);
					UE_LOG(LogInit, Display, TEXT("Failure - %d error(s), %d warning(s)"), GWarn->Errors.Num(), GWarn->Warnings.Num() );
					ErrorLevel = 1;
				}
				CLEAR_WARN_COLOR();
			}
			else
			{
				UE_LOG(LogInit, Display, TEXT("Finished.") );
			}
		
			double CommandletExecutionTime = FPlatformTime::Seconds() - CommandletExecutionStartTime;
			UE_LOG(LogInit, Display, LINE_TERMINATOR TEXT( "Execution of commandlet took:  %.2f seconds"), CommandletExecutionTime );

			// We're ready to exit!
			return ErrorLevel;
		}
		else
		{
			// We're a regular client.
			check(bIsRegularClient);

			if (bTokenDoesNotHaveDash)
			{
				// here we give people a reasonable warning if they tried to use the short name of a commandlet
				UClass* TempCommandletClass = FindObject<UClass>(ANY_PACKAGE,*(Token+TEXT("Commandlet")),false);
				if (TempCommandletClass)
				{
					UE_LOG(LogInit, Fatal, TEXT("You probably meant to call a commandlet. Please use the full name %s."), *(Token+TEXT("Commandlet")));
				}
			}
		}
	}

	// exit if wanted.
	if( GIsRequestingExit )
	{
		if ( GEngine != NULL )
		{
			GEngine->PreExit();
		}
		AppPreExit();
		// appExit is called outside guarded block.
		return 1;
	}

	FString MatineeName;

	if(FParse::Param(FCommandLine::Get(),TEXT("DUMPMOVIE")) || FParse::Value(FCommandLine::Get(), TEXT("-MATINEESSCAPTURE="), MatineeName))
	{
		// -1: remain on
		GIsDumpingMovie = -1;
	}

	// If dumping movie then we do NOT want on-screen messages
	GAreScreenMessagesEnabled = !GIsDumpingMovie && !GIsDemoMode;

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(),TEXT("NOSCREENMESSAGES")))
	{
		GAreScreenMessagesEnabled = false;
	}

	// Don't update INI files if benchmarking or -noini
	if( FApp::IsBenchmarking() || FParse::Param(FCommandLine::Get(),TEXT("NOINI")))
	{
		GConfig->Detach( GEngineIni );
		GConfig->Detach( GInputIni );
		GConfig->Detach( GGameIni );
		GConfig->Detach( GEditorIni );
	}
#endif // !UE_BUILD_SHIPPING

	delete [] CommandLineCopy;

	// initialize the pointer, as it is deleted before being assigned in the first frame
	PendingCleanupObjects = NULL;

	// Initialize profile visualizers.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FModuleManager::Get().LoadModule(TEXT("TaskGraph"));
	if (FPlatformProcess::SupportsMultithreading())
	{
		FModuleManager::Get().LoadModule(TEXT("ProfilerService"));
		FModuleManager::Get().GetModuleChecked<IProfilerServiceModule>("ProfilerService").CreateProfilerServiceManager();
	}
#endif

	// Init HighRes screenshot system.
	GetHighResScreenshotConfig().Init();

#else // WITH_ENGINE
	EndInitTextLocalization();
	FPlatformMisc::PlatformPostInit();
#endif // WITH_ENGINE

	//run automation smoke tests now that everything is setup to run
	FAutomationTestFramework::GetInstance().RunSmokeTests();

	// Note we still have 20% remaining on the slow task: this will be used by the Editor/Engine initialization next
	return 0;
}

void FEngineLoop::LoadCoreModules()
{
	// Always attempt to load CoreUObject. It requires additional pre-init which is called from its module's StartupModule method.
#if WITH_COREUOBJECT
	FModuleManager::Get().LoadModule(TEXT("CoreUObject"));
#endif
}

void FEngineLoop::LoadPreInitModules()
{	
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading PreInit Modules"), STAT_PreInitModules, STATGROUP_LoadTime);

	// GGetMapNameDelegate is initialized here
#if WITH_ENGINE
	FModuleManager::Get().LoadModule(TEXT("Engine"));

	FModuleManager::Get().LoadModule(TEXT("Renderer"));

	FPlatformMisc::LoadPreInitModules();
	
#if !UE_SERVER
	if (!IsRunningDedicatedServer() )
	{
		// This needs to be loaded before InitializeShaderTypes is called
		FModuleManager::Get().LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer");
	}
#endif

	FModuleManager::Get().LoadModule(TEXT("Landscape"));

	// Initialize ShaderCore before loading or compiling any shaders,
	// But after Renderer and any other modules which implement shader types.
	FModuleManager::Get().LoadModule(TEXT("ShaderCore"));
	
#if WITH_EDITORONLY_DATA
	// Load the texture compressor module before any textures load. They may
	// compress asynchronously and that can lead to a race condition.
	FModuleManager::Get().LoadModule(TEXT("TextureCompressor"));
#endif
#endif // WITH_ENGINE
}

#if WITH_ENGINE

bool FEngineLoop::LoadStartupCoreModules()
{
	FScopedSlowTask SlowTask(100);

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Startup Modules"), STAT_StartupModules, STATGROUP_LoadTime);

	bool bSuccess = true;

	// Load all Runtime modules
	SlowTask.EnterProgressFrame(10);
	{
		FModuleManager::Get().LoadModule(TEXT("Core"));
		FModuleManager::Get().LoadModule(TEXT("Networking"));
	}

	SlowTask.EnterProgressFrame(10);
	FPlatformMisc::LoadStartupModules();

	// initialize messaging
	SlowTask.EnterProgressFrame(10);
	if (FPlatformProcess::SupportsMultithreading())
	{
		FModuleManager::LoadModuleChecked<IMessagingModule>("Messaging");

		if (!IsRunningCommandlet())
		{
			SessionService = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices").GetSessionService();
			SessionService->Start();
		}

		EngineService = new FEngineService();
	}

	SlowTask.EnterProgressFrame(10);
#if WITH_EDITOR
		FModuleManager::LoadModuleChecked<IEditorStyleModule>("EditorStyle");
#endif //WITH_EDITOR

	// Load UI modules
	SlowTask.EnterProgressFrame(10);
	if ( !IsRunningDedicatedServer() )
	{
		FModuleManager::Get().LoadModule("Slate");
	}

#if WITH_EDITOR
	// In dedicated server builds with the editor, we need to load UMG/UMGEditor for compiling blueprints.
	// UMG must be loaded for runtime and cooking.
	FModuleManager::Get().LoadModule("UMG");
#else
	if ( !IsRunningDedicatedServer() )
	{
		// UMG must be loaded for runtime and cooking.
		FModuleManager::Get().LoadModule("UMG");
	}
#endif //WITH_EDITOR

	// Load all Development modules
	SlowTask.EnterProgressFrame(20);
	if (!IsRunningDedicatedServer())
	{
#if WITH_UNREAL_DEVELOPER_TOOLS
		FModuleManager::Get().LoadModule("MessageLog");
		FModuleManager::Get().LoadModule("CollisionAnalyzer");
#endif	//WITH_UNREAL_DEVELOPER_TOOLS
	}

#if WITH_UNREAL_DEVELOPER_TOOLS
		FModuleManager::Get().LoadModule("FunctionalTesting");
#endif	//WITH_UNREAL_DEVELOPER_TOOLS

	SlowTask.EnterProgressFrame(30);
#if (WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))
	// HACK: load BT editor as early as possible for statically initialized assets (non cooked BT assets needs it)
	// cooking needs this module too
	FModuleManager::Get().LoadModule(TEXT("BehaviorTreeEditor"));

	// -----------------------------------------------------

	// HACK: load AbilitySystem editor as early as possible for statically initialized assets (non cooked BT assets needs it)
	// cooking needs this module too
	bool bGameplayAbilitiesEnabled = false;
	GConfig->GetBool(TEXT("GameplayAbilities"), TEXT("GameplayAbilitiesEditorEnabled"), bGameplayAbilitiesEnabled, GEngineIni);
	if (bGameplayAbilitiesEnabled)
	{
		FModuleManager::Get().LoadModule(TEXT("GameplayAbilitiesEditor"));
	}

	// -----------------------------------------------------

	// HACK: load EQS editor as early as possible for statically initialized assets (non cooked EQS assets needs it)
	// cooking needs this module too
	bool bEnvironmentQueryEditor = false;
	GConfig->GetBool(TEXT("EnvironmentQueryEd"), TEXT("EnableEnvironmentQueryEd"), bEnvironmentQueryEditor, GEngineIni);
	if (bEnvironmentQueryEditor 
#if WITH_EDITOR
		|| GetDefault<UEditorExperimentalSettings>()->bEQSEditor
#endif // WITH_EDITOR
		)
	{
		FModuleManager::Get().LoadModule(TEXT("EnvironmentQueryEditor"));
	}

	// We need this for blueprint projects that have online functionality.
	FModuleManager::Get().LoadModule(TEXT("OnlineBlueprintSupport"));

#endif //(WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

	return bSuccess;
}

bool FEngineLoop::LoadStartupModules()
{
	FScopedSlowTask SlowTask(3);

	SlowTask.EnterProgressFrame(1);
	// Load any modules that want to be loaded before default modules are loaded up.
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PreDefault) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault))
	{
		return false;
	}

	SlowTask.EnterProgressFrame(1);
	// Load modules that are configured to load in the default phase
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::Default) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::Default))
	{
		return false;
	}

	SlowTask.EnterProgressFrame(1);
	// Load any modules that want to be loaded after default modules are loaded up.
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PostDefault) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PostDefault))
	{
		return false;
	}

	return true;
}


void FEngineLoop::InitTime()
{
	// Init variables used for benchmarking and ticking.
	FApp::SetCurrentTime(FPlatformTime::Seconds());
	MaxFrameCounter				= 0;
	MaxTickTime					= 0;
	TotalTickTime				= 0;
	LastFrameCycles				= FPlatformTime::Cycles();

	float FloatMaxTickTime		= 0;
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(),TEXT("SECONDS="),FloatMaxTickTime);
	MaxTickTime					= FloatMaxTickTime;

	// look of a version of seconds that only is applied if FApp::IsBenchmarking() is set. This makes it easier on
	// say, iOS, where we have a toggle setting to enable benchmarking, but don't want to have to make user
	// also disable the seconds setting as well. -seconds= will exit the app after time even if benchmarking
	// is not enabled
	// NOTE: This will override -seconds= if it's specified
	if (FApp::IsBenchmarking())
	{
		if (FParse::Value(FCommandLine::Get(),TEXT("BENCHMARKSECONDS="),FloatMaxTickTime) && FloatMaxTickTime)
		{
			MaxTickTime			= FloatMaxTickTime;
		}
	}

	// Use -FPS=X to override fixed tick rate if e.g. -BENCHMARK is used.
	float FixedFPS = 0;
	FParse::Value(FCommandLine::Get(),TEXT("FPS="),FixedFPS);
	if( FixedFPS > 0 )
	{
		GEngine->MatineeScreenshotOptions.MatineeCaptureFPS = (int32)FixedFPS;
		FApp::SetFixedDeltaTime(1 / FixedFPS);
	}

#endif // !UE_BUILD_SHIPPING

	// convert FloatMaxTickTime into number of frames (using 1 / FApp::GetFixedDeltaTime() to convert fps to seconds )
	MaxFrameCounter = FMath::TruncToInt(MaxTickTime / FApp::GetFixedDeltaTime());
}



//called via FCoreDelegates::StarvedGameLoop
void GameLoopIsStarved()
{
	FlushPendingDeleteRHIResources_GameThread();
	FStats::AdvanceFrame( true, FStats::FOnAdvanceRenderingThreadStats::CreateStatic( &AdvanceRenderingThreadStatsGT ) );
}


int32 FEngineLoop::Init()
{
	FScopedSlowTask SlowTask(100);
	SlowTask.EnterProgressFrame(10);

	// Figure out which UEngine variant to use.
	UClass* EngineClass = NULL;
	if( !GIsEditor )
	{
		// We're the game.
		EngineClass = StaticLoadClass( UGameEngine::StaticClass(), NULL, TEXT("engine-ini:/Script/Engine.Engine.GameEngine"), NULL, LOAD_None, NULL );
		GEngine = NewObject<UEngine>(GetTransientPackage(), EngineClass);
	}
	else
	{
#if WITH_EDITOR
		// We're UnrealEd.
		EngineClass = StaticLoadClass( UUnrealEdEngine::StaticClass(), NULL, TEXT("engine-ini:/Script/Engine.Engine.UnrealEdEngine"), NULL, LOAD_None, NULL );
		GEngine = GEditor = GUnrealEd = NewObject<UUnrealEdEngine>(GetTransientPackage(), EngineClass);
#else
		check(0);
#endif
	}

	check( GEngine );
	
	GetMoviePlayer()->PassLoadingScreenWindowBackToGame();

	GEngine->ParseCommandline();

	InitTime();

	SlowTask.EnterProgressFrame(60);

	GEngine->Init(this);

	SlowTask.EnterProgressFrame(30);

	// Load all the post-engine init modules
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PostEngineInit) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PostEngineInit))
	{
		GIsRequestingExit = true;
		return 1;
	}
	
	GetMoviePlayer()->WaitForMovieToFinish();

	// initialize automation worker
#if WITH_AUTOMATION_WORKER
	FModuleManager::Get().LoadModule("AutomationWorker");
#endif

#if WITH_EDITOR
	if( GIsEditor )
	{
		FModuleManager::GetModuleChecked<IAutomationControllerModule>("AutomationController").Init();
		FModuleManager::Get().LoadModule(TEXT("ProfilerClient"));
	}
#endif

	GIsRunning = true;

	if (!GIsEditor)
	{
		// hide a couple frames worth of rendering
		FViewport::SetGameRenderingEnabled(true, 3);
	}

	// Begin the async platform hardware survey
	GEngine->InitHardwareSurvey();

	FCoreDelegates::StarvedGameLoop.BindStatic(&GameLoopIsStarved);

	return 0;
}


void FEngineLoop::Exit()
{
	GIsRunning	= 0;
	GLogConsole	= NULL;

	// shutdown visual logger and flush all data
#if ENABLE_VISUAL_LOG
	FVisualLogger::Get().Shutdown();
#endif

	GetMoviePlayer()->Shutdown();

	// Make sure we're not in the middle of loading something.
	FlushAsyncLoading();

	// Block till all outstanding resource streaming requests are fulfilled.
	if (!IStreamingManager::HasShutdown())
	{
		UTexture2D::CancelPendingTextureStreaming();
		IStreamingManager::Get().BlockTillAllRequestsFinished();
	}

#if WITH_ENGINE
	// shut down messaging
	delete EngineService;
	EngineService = NULL;

	if (SessionService.IsValid())
	{
		SessionService->Stop();
		SessionService.Reset();
	}

	if (GDistanceFieldAsyncQueue)
	{
		GDistanceFieldAsyncQueue->Shutdown();
		delete GDistanceFieldAsyncQueue;
	}
#endif // WITH_ENGINE

	MALLOC_PROFILER( GEngine->Exec( NULL, TEXT( "MPROF STOP" ) ); )

	if ( GEngine != NULL )
	{
		GEngine->ShutdownAudioDeviceManager();
	}

	if ( GEngine != NULL )
	{
		GEngine->PreExit();
	}

#if !UE_SERVER
	if ( FEngineFontServices::IsInitialized() )
	{
		FEngineFontServices::Destroy();
	}
#endif

	// close all windows
	FSlateApplication::Shutdown();

	AppPreExit();

	TermGamePhys();
	ParticleVertexFactoryPool_FreePool();
	MotionBlur_Free();

	// Stop the rendering thread.
	StopRenderingThread();

	// Tear down the RHI.
	RHIExit();

#if WITH_ENGINE
	// Save the hot reload state
	IHotReloadInterface* HotReload = IHotReloadInterface::GetPtr();
	if(HotReload != nullptr)
	{
		HotReload->SaveConfig();
	}
#endif

	// Unload all modules.  Note that this doesn't actually unload the module DLLs (that happens at
	// process exit by the OS), but it does call ShutdownModule() on all loaded modules in the reverse
	// order they were loaded in, so that systems can unregister and perform general clean up.
	FModuleManager::Get().UnloadModulesAtShutdown();

#if STATS
	FThreadStats::StopThread();
#endif

	FTaskGraphInterface::Shutdown();

	IStreamingManager::Shutdown();

	FIOSystem::Shutdown();
}

void FEngineLoop::ProcessLocalPlayerSlateOperations() const
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// For all the game worlds drill down to the player controller for each game viewport and process it's slate operation
	for ( const FWorldContext& Context : GEngine->GetWorldContexts() )
	{
		UWorld* CurWorld = Context.World();
		if ( CurWorld && CurWorld->IsGameWorld() )
		{
			UGameViewportClient* GameViewportClient = CurWorld->GetGameViewport();
			TSharedPtr< SViewport > ViewportWidget = GameViewportClient ? GameViewportClient->GetGameViewportWidget() : nullptr;

			if ( ViewportWidget.IsValid() )
			{
				for( FConstPlayerControllerIterator Iterator = CurWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
				{
					APlayerController* PlayerController = *Iterator;
					if( PlayerController )
					{
						ULocalPlayer* LocalPlayer = Cast< ULocalPlayer >( PlayerController->Player );
						if( LocalPlayer )
						{
							FReply& TheReply = LocalPlayer->GetSlateOperations();

							FWidgetPath PathToWidget;
							SlateApp.GeneratePathToWidgetUnchecked( ViewportWidget.ToSharedRef(), PathToWidget );

							SlateApp.ProcessReply( PathToWidget, TheReply, nullptr, nullptr, LocalPlayer->GetControllerId() );

							TheReply = FReply::Unhandled();
						}
					}
				}
			}
		}
	}
}

bool FEngineLoop::ShouldUseIdleMode() const
{
	static const auto CVarIdleWhenNotForeground = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("t.IdleWhenNotForeground"));
	bool bIdleMode = false;

	// Yield cpu usage if desired
	if (FApp::IsGame()
		&& FPlatformProperties::SupportsWindowedMode()
		&& CVarIdleWhenNotForeground->GetValueOnGameThread()
		&& !FPlatformProcess::IsThisApplicationForeground())
	{
		bIdleMode = true;

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (!Context.World()->AreAlwaysLoadedLevelsLoaded())
			{
				bIdleMode = false;
				break;
			}
		}
	}

	return bIdleMode;
}

void FEngineLoop::Tick()
{
	// Ensure we aren't starting a frame while loading or playing a loading movie
	ensure(GetMoviePlayer()->IsLoadingFinished() && !GetMoviePlayer()->IsMovieCurrentlyPlaying());

	// early in the Tick() to get the callbacks for cvar changes called
	{
#if WITH_ENGINE
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_CallAllConsoleVariableSinks);
#endif
		IConsoleManager::Get().CallAllConsoleVariableSinks();
	}

	{ 
		SCOPE_CYCLE_COUNTER( STAT_FrameTime );	

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			BeginFrame,
		{
			GRHICommandList.LatchBypass();
			GFrameNumberRenderThread++;
			RHICmdList.PushEvent(*FString::Printf(TEXT("Frame%d"),GFrameNumberRenderThread));
			RHICmdList.BeginFrame();
		});

		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_FlushThreadedLogs);
#endif
			// Flush debug output which has been buffered by other threads.
			GLog->FlushThreadedLogs();
		}

		// Exit if frame limit is reached in benchmark mode.
		if( (FApp::IsBenchmarking() && MaxFrameCounter && (GFrameCounter > MaxFrameCounter))
			// or time limit is reached if set.
			|| (MaxTickTime && (TotalTickTime > MaxTickTime)) )
		{
			FPlatformMisc::RequestExit(0);
		}

		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_UpdateTimeAndHandleMaxTickRate);
#endif
			// Set FApp::CurrentTime, FApp::DeltaTime and potentially wait to enforce max tick rate.
			GEngine->UpdateTimeAndHandleMaxTickRate();
		}

		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_TickFPSChart);
#endif
			GEngine->TickFPSChart( FApp::GetDeltaTime() );
		}

#if WITH_ENGINE
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Malloc_UpdateStats);
#endif
		// Update platform memory and memory allocator stats.
		FPlatformMemory::UpdateStats();
		GMalloc->UpdateStats();
	} 

	FStats::AdvanceFrame( false, FStats::FOnAdvanceRenderingThreadStats::CreateStatic( &AdvanceRenderingThreadStatsGT ) );

	{ 
		SCOPE_CYCLE_COUNTER( STAT_FrameTime );

		// Calculates average FPS/MS (outside STATS on purpose)
		CalculateFPSTimings();

		// Note the start of a new frame
		MALLOC_PROFILER(GMalloc->Exec( NULL, *FString::Printf(TEXT("SNAPSHOTMEMORYFRAME")),*GLog));

		// handle some per-frame tasks on the rendering thread
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			ResetDeferredUpdates,
			{
				FDeferredUpdateResource::ResetNeedsUpdate();
				FlushPendingDeleteRHIResources_RenderThread();
			});
		
		{
#if WITH_ENGINE
			SCOPE_CYCLE_COUNTER( STAT_PlatformMessageTime );
#endif
			SCOPE_CYCLE_COUNTER(STAT_PumpMessages);
			FPlatformMisc::PumpMessages(true);
		}

		bool bIdleMode;
		{

#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Idle);
#endif
			// Idle mode prevents ticking and rendering completely
			bIdleMode = ShouldUseIdleMode();
			if (bIdleMode)
			{
				// Yield CPU time
				FPlatformProcess::Sleep(.1f);
			}
		}

		if (FSlateApplication::IsInitialized() && !bIdleMode)
		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_SlateInput);
#endif
			FSlateApplication& SlateApp = FSlateApplication::Get();
			SlateApp.PollGameDeviceState();
			// Gives widgets a chance to process any accumulated input
			SlateApp.FinishedInputThisFrame();
		}

		GEngine->Tick( FApp::GetDeltaTime(), bIdleMode );
		
		// If a movie that is blocking the game thread has been playing,
		// wait for it to finish before we continue to tick or tick again
		// We do this right after GEngine->Tick() because that is where user code would initiate a load / movie.
		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_WaitForMovieToFinish);
#endif
			GetMoviePlayer()->WaitForMovieToFinish();
		}
		
		if (GShaderCompilingManager)
		{
			// Process any asynchronous shader compile results that are ready, limit execution time
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_GShaderCompilingManager);
#endif
			GShaderCompilingManager->ProcessAsyncResults(true, false);
		}

		if (GDistanceFieldAsyncQueue)
		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_GDistanceFieldAsyncQueue);
#endif
			GDistanceFieldAsyncQueue->ProcessAsyncTasks();
		}

		if (FSlateApplication::IsInitialized() && !bIdleMode)
		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_ProcessPlayerControllersSlateOperations);
#endif
			check(!IsRunningDedicatedServer());
			
			// Process slate operations accumulated in the world ticks.
			ProcessLocalPlayerSlateOperations();

			// Tick Slate application
			FSlateApplication::Get().Tick();
		}

#if STATS
		// Clear any stat group notifications we have pending just incase they weren't claimed during FSlateApplication::Get().Tick
		extern CORE_API void ClearPendingStatGroups();
		ClearPendingStatGroups();
#endif

#if WITH_EDITOR
		if( GIsEditor )
		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_AutomationController);
#endif
			static FName AutomationController("AutomationController");
			//Check if module loaded to support the change to allow this to be hot compilable.
			if (FModuleManager::Get().IsModuleLoaded(AutomationController))
			{
				FModuleManager::GetModuleChecked<IAutomationControllerModule>(AutomationController).Tick();
			}
		}
#endif

#if WITH_ENGINE
#if WITH_AUTOMATION_WORKER
		{
#if WITH_ENGINE
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FEngineLoop_Tick_AutomationWorker);
#endif
			//Check if module loaded to support the change to allow this to be hot compilable.
			static const FName AutomationWorkerModuleName = TEXT("AutomationWorker");
			if (FModuleManager::Get().IsModuleLoaded(AutomationWorkerModuleName))
			{
				FModuleManager::GetModuleChecked<IAutomationWorkerModule>(AutomationWorkerModuleName).Tick();
			}
		}
#endif
#endif //WITH_ENGINE

		{			
#if WITH_ENGINE
			SCOPE_CYCLE_COUNTER(STAT_RHITickTime);
#endif
			RHITick( FApp::GetDeltaTime() ); // Update RHI.
		}

		// Increment global frame counter. Once for each engine tick.
		GFrameCounter++;

		// Disregard first few ticks for total tick time as it includes loading and such.
		if( GFrameCounter > 6 )
		{
			TotalTickTime+=FApp::GetDeltaTime();
		}


		// Find the objects which need to be cleaned up the next frame.
		FPendingCleanupObjects* PreviousPendingCleanupObjects = PendingCleanupObjects;
		PendingCleanupObjects = GetPendingCleanupObjects();

		{
#if WITH_ENGINE
			SCOPE_CYCLE_COUNTER( STAT_FrameSyncTime );
#endif
			// this could be perhaps moved down to get greater parallelizm
			// Sync game and render thread. Either total sync or allowing one frame lag.
			static FFrameEndSync FrameEndSync;
			static auto CVarAllowOneFrameThreadLag = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.OneFrameThreadLag"));
			FrameEndSync.Sync( CVarAllowOneFrameThreadLag->GetValueOnGameThread() != 0 );
		}

		{
#if WITH_ENGINE
			SCOPE_CYCLE_COUNTER( STAT_DeferredTickTime );
#endif

			// Delete the objects which were enqueued for deferred cleanup before the previous frame.
			delete PreviousPendingCleanupObjects;

			FTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());

			FSingleThreadManager::Get().Tick();

			GEngine->TickDeferredCommands();		
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			EndFrame,
		{
			FShaderCache::PreDrawShaders(RHICmdList);
			RHICmdList.EndFrame();
			RHICmdList.PopEvent();
		});
	} 

	// Check for async platform hardware survey results
	GEngine->TickHardwareSurvey();

	// Set CPU utilization stats.
	const FCPUTime CPUTime = FPlatformTime::GetCPUTime();
	SET_FLOAT_STAT(STAT_CPUTimePct,CPUTime.CPUTimePct);
	SET_FLOAT_STAT(STAT_CPUTimePctRelative,CPUTime.CPUTimePctRelative);
}

void FEngineLoop::ClearPendingCleanupObjects()
{
	delete PendingCleanupObjects;
	PendingCleanupObjects = NULL;
}

#endif // WITH_ENGINE

static void CheckForPrintTimesOverride()
{
	GPrintLogTimes = ELogTimes::None;

	// Determine whether to override the default setting for including timestamps in the log.
	FString LogTimes;
	if (GConfig->GetString( TEXT( "LogFiles" ), TEXT( "LogTimes" ), LogTimes, GEngineIni ))
	{
		if (LogTimes == TEXT( "SinceStart" ))
		{
			GPrintLogTimes = ELogTimes::SinceGStartTime;
		}
		// Assume this is a bool for backward compatibility
		else if (FCString::ToBool( *LogTimes ))
		{
			GPrintLogTimes = ELogTimes::UTC;
		}
	}

	if (FParse::Param( FCommandLine::Get(), TEXT( "LOGTIMES" ) ))
	{
		GPrintLogTimes = ELogTimes::UTC;
	}
	else if (FParse::Param( FCommandLine::Get(), TEXT( "NOLOGTIMES" ) ))
	{
		GPrintLogTimes = ELogTimes::None;
	}
	else if (FParse::Param( FCommandLine::Get(), TEXT( "LOGTIMESINCESTART" ) ))
	{
		GPrintLogTimes = ELogTimes::SinceGStartTime;
	}
}

/* FEngineLoop static interface
 *****************************************************************************/

bool FEngineLoop::AppInit( )
{
	// Output devices.
	GError = FPlatformOutputDevices::GetError();
	GWarn = FPlatformOutputDevices::GetWarn();

	BeginInitTextLocalization();

	// Avoiding potential exploits by not exposing command line overrides in the shipping games.
#if !UE_BUILD_SHIPPING && WITH_EDITORONLY_DATA
	// 8192 is the maximum length of the command line on Windows XP.
	TCHAR CmdLineEnv[8192];

	// Retrieve additional command line arguments from environment variable.
	FPlatformMisc::GetEnvironmentVariable(TEXT("UE-CmdLineArgs"), CmdLineEnv,ARRAY_COUNT(CmdLineEnv));
	
	// Manually NULL terminate just in case. The NULL string is returned above in the error case so
	// we don't have to worry about that.
	CmdLineEnv[ARRAY_COUNT(CmdLineEnv)-1] = 0;
	FString Env = FString(CmdLineEnv).Trim();

	if (Env.Len())
	{
		// Append the command line environment after inserting a space as we can't set it in the 
		// environment. Note that any code accessing GCmdLine before appInit obviously won't 
		// respect the command line environment additions.
		FCommandLine::Append(TEXT(" -EnvAfterHere "));
		FCommandLine::Append(CmdLineEnv);
	}
#endif

	// Error history.
	FCString::Strcpy(GErrorHist, TEXT("Fatal error!" LINE_TERMINATOR LINE_TERMINATOR));

	// Platform specific pre-init.
	FPlatformMisc::PlatformPreInit();

	// Keep track of start time.
	GSystemStartTime = FDateTime::Now().ToString();

	// Switch into executable's directory.
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	// Now finish initializing the file manager after the command line is set up
	IFileManager::Get().ProcessCommandLineOptions();

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("BUILDMACHINE")))
	{
		GIsBuildMachine = true;
	}

	// If "-WaitForDebugger" was specified, halt startup and wait for a debugger to attach before continuing
	if( FParse::Param( FCommandLine::Get(), TEXT( "WaitForDebugger" ) ) )
	{
		while( !FPlatformMisc::IsDebuggerPresent() )
		{
			FPlatformProcess::Sleep( 0.1f );
		}
	}
#endif // !UE_BUILD_SHIPPING

#if PLATFORM_WINDOWS

	// make sure that the log directory exists
	IFileManager::Get().MakeDirectory( *FPaths::GameLogDir() );

	// update the mini dump filename now that we have enough info to point it to the log folder even in installed builds
	FCString::Strcpy(MiniDumpFilenameW, *IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FString::Printf(TEXT("%sunreal-v%i-%s.dmp"), *FPaths::GameLogDir(), GEngineVersion.GetChangelist(), *FDateTime::Now().ToString())));
#endif

	// Init logging to disk
	FPlatformOutputDevices::SetupOutputDevices();

	// init config system
	FConfigCacheIni::InitializeConfigSystem();

	CheckForPrintTimesOverride();

	// Check whether the project or any of its plugins are missing or are out of date
#if UE_EDITOR
	if(!GIsBuildMachine && FPaths::IsProjectFilePathSet() && IPluginManager::Get().AreRequiredPluginsAvailable())
	{
		const FProjectDescriptor* CurrentProject = IProjectManager::Get().GetCurrentProject();
		if(CurrentProject != nullptr && CurrentProject->Modules.Num() > 0)
		{
			bool bNeedCompile = false;
			GConfig->GetBool(TEXT("/Script/UnrealEd.EditorLoadingSavingSettings"), TEXT("bForceCompilationAtStartup"), bNeedCompile, GEditorPerProjectIni);

			if(!bNeedCompile)
			{
				// Check if any of the project or plugin modules are out of date, and the user wants to compile them.
				TArray<FString> IncompatibleFiles;
				IProjectManager::Get().CheckModuleCompatibility(IncompatibleFiles);
				IPluginManager::Get().CheckModuleCompatibility(IncompatibleFiles);

				if (IncompatibleFiles.Num() > 0)
				{
					// Log the modules which need to be rebuilt
					FString ModulesList = TEXT("The following modules are missing or built with a different engine version:\n\n");
					for (int Idx = 0; Idx < IncompatibleFiles.Num(); Idx++)
					{
						UE_LOG(LogInit, Warning, TEXT("Incompatible or missing module: %s"), *IncompatibleFiles[Idx]);
						ModulesList += IncompatibleFiles[Idx] + TEXT("\n");
					}
					ModulesList += TEXT("\nWould you like to rebuild them now?");

					// If we're running with -stdout, assume that we're a non interactive process and about to fail
					if (FApp::IsUnattended() || FParse::Param(FCommandLine::Get(), TEXT("stdout")))
					{
						return false;
					}

					// Ask whether to compile before continuing
					if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *ModulesList, *FString::Printf(TEXT("Missing %s Modules"), FApp::GetGameName())) == EAppReturnType::No)
					{
						return false;
					}

					bNeedCompile = true;
				}
			}

			if(bNeedCompile)
			{
				// Try to compile it
				FFeedbackContext *Context = (FFeedbackContext*)FDesktopPlatformModule::Get()->GetNativeFeedbackContext();
				Context->BeginSlowTask(FText::FromString(TEXT("Starting build...")), true, true);
				bool bCompileResult = FDesktopPlatformModule::Get()->CompileGameProject(FPaths::RootDir(), FPaths::GetProjectFilePath(), Context);
				Context->EndSlowTask();

				// Get a list of modules which are still incompatible
				TArray<FString> StillIncompatibleFiles;
				IProjectManager::Get().CheckModuleCompatibility(StillIncompatibleFiles);
				IPluginManager::Get().CheckModuleCompatibility(StillIncompatibleFiles);

				if(!bCompileResult || StillIncompatibleFiles.Num() > 0)
				{
					for (int Idx = 0; Idx < StillIncompatibleFiles.Num(); Idx++)
					{
						UE_LOG(LogInit, Warning, TEXT("Still incompatible or missing module: %s"), *StillIncompatibleFiles[Idx]);
					}
					FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT("%s could not be compiled. Try rebuilding from source manually."), FApp::GetGameName()), TEXT("Error"));
					return false;
				}
			}
		}
	}
#endif

	// Load "pre-init" plugin modules
	if (!IProjectManager::Get().LoadModulesForProject(ELoadingPhase::PostConfigInit) || !IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PostConfigInit))
	{
		return false;
	}

	PreInitHMDDevice();

	// Put the command line and config info into the suppression system
	FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();

	// after the above has run we now have the REQUIRED set of engine .INIs  (all of the other .INIs)
	// that are gotten from .h files' config() are not requires and are dynamically loaded when the .u files are loaded

#if !UE_BUILD_SHIPPING
	// Prompt the user for remote debugging?
	bool bPromptForRemoteDebug = false;
	GConfig->GetBool(TEXT("Engine.ErrorHandling"), TEXT("bPromptForRemoteDebugging"), bPromptForRemoteDebug, GEngineIni);
	bool bPromptForRemoteDebugOnEnsure = false;
	GConfig->GetBool(TEXT("Engine.ErrorHandling"), TEXT("bPromptForRemoteDebugOnEnsure"), bPromptForRemoteDebugOnEnsure, GEngineIni);

	if (FParse::Param(FCommandLine::Get(), TEXT("PROMPTREMOTEDEBUG")))
	{
		bPromptForRemoteDebug = true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("PROMPTREMOTEDEBUGENSURE")))
	{
		bPromptForRemoteDebug = true;
		bPromptForRemoteDebugOnEnsure = true;
	}

	FPlatformMisc::SetShouldPromptForRemoteDebugging(bPromptForRemoteDebug);
	FPlatformMisc::SetShouldPromptForRemoteDebugOnEnsure(bPromptForRemoteDebugOnEnsure);

	// Feedback context.
	if (FParse::Param(FCommandLine::Get(), TEXT("WARNINGSASERRORS")))
	{
		GWarn->TreatWarningsAsErrors = true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("SILENT")))
	{
		GIsSilent = true;
	}

	// Show log if wanted.
	if (GLogConsole && FParse::Param(FCommandLine::Get(), TEXT("LOG")))
	{
		GLogConsole->Show(true);
	}

#endif // !UE_BUILD_SHIPPING

	//// Command line.
	UE_LOG(LogInit, Log, TEXT("Version: %s"), *GEngineVersion.ToString());
	UE_LOG(LogInit, Log, TEXT("API Version: %u"), MODULE_API_VERSION);

#if PLATFORM_64BITS
	UE_LOG(LogInit, Log, TEXT("Compiled (64-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__));
#else
	UE_LOG(LogInit, Log, TEXT("Compiled (32-bit): %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__));
#endif

	// Print compiler version info
#if defined(__clang__)
	UE_LOG(LogInit, Log, TEXT("Compiled with Clang: %s"), ANSI_TO_TCHAR( __clang_version__ ) );
#elif defined( _MSC_VER )
	#ifndef __INTELLISENSE__	// Intellisense compiler doesn't support _MSC_FULL_VER
	{
		const FString VisualCPPVersion( FString::Printf( TEXT( "%d" ), _MSC_FULL_VER ) );
		const FString VisualCPPRevisionNumber( FString::Printf( TEXT( "%02d" ), _MSC_BUILD ) );
		UE_LOG(LogInit, Log, TEXT("Compiled with Visual C++: %s.%s.%s.%s"), 
			*VisualCPPVersion.Mid( 0, 2 ), // Major version
			*VisualCPPVersion.Mid( 2, 2 ), // Minor version
			*VisualCPPVersion.Mid( 4 ),	// Build version
			*VisualCPPRevisionNumber	// Revision number
			);
	}
	#endif
#else
	UE_LOG(LogInit, Log, TEXT("Compiled with unrecognized C++ compiler") );
#endif

	UE_LOG(LogInit, Log, TEXT("Build Configuration: %s"), EBuildConfigurations::ToString(FApp::GetBuildConfiguration()));
	UE_LOG(LogInit, Log, TEXT("Branch Name: %s"), *FApp::GetBranchName() );
	UE_LOG(LogInit, Log, TEXT("Command line: %s"), FCommandLine::Get() );
	UE_LOG(LogInit, Log, TEXT("Base directory: %s"), FPlatformProcess::BaseDir() );
	//UE_LOG(LogInit, Log, TEXT("Character set: %s"), sizeof(TCHAR)==1 ? TEXT("ANSI") : TEXT("Unicode") );
	UE_LOG(LogInit, Log, TEXT("Rocket: %d"), FRocketSupport::IsRocket()? 1 : 0);

	// if a logging build, clear out old log files
#if !NO_LOGGING && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FMaintenance::DeleteOldLogs();
#endif

#if !UE_BUILD_SHIPPING
	FApp::InitializeSession();
#endif

#if WITH_ENGINE
	// Earliest place to init the online subsystems
	// Code needs GConfigFile to be valid
	// Must be after FThreadStats::StartThread();
	// Must be before Render/RHI subsystem D3DCreate()
	// For platform services that need D3D hooks like Steam
	// --
	// Why load HTTP?
	// Because most, if not all online subsystems will load HTTP themselves. This can cause problems though, as HTTP will be loaded *after* OSS, 
	// and if OSS holds on to resources allocated by it, this will cause crash (modules are being unloaded in LIFO order with no dependency tracking).
	// Loading HTTP before OSS works around this problem by making ModuleManager unload HTTP after OSS, at the cost of extra module for the few OSS (like Null) that don't use it.
	FModuleManager::Get().LoadModule(TEXT("HTTP"));
	FModuleManager::Get().LoadModule(TEXT("OnlineSubsystem"));
	FModuleManager::Get().LoadModule(TEXT("OnlineSubsystemUtils"));
#endif

	// Checks.
	check(sizeof(uint8) == 1);
	check(sizeof(int8) == 1);
	check(sizeof(uint16) == 2);
	check(sizeof(uint32) == 4);
	check(sizeof(uint64) == 8);
	check(sizeof(ANSICHAR) == 1);

#if PLATFORM_TCHAR_IS_4_BYTES
	check(sizeof(TCHAR) == 4);
#else
	check(sizeof(TCHAR) == 2);
#endif

	check(sizeof(int16) == 2);
	check(sizeof(int32) == 4);
	check(sizeof(int64) == 8);
	check(sizeof(bool) == 1);
	check(sizeof(float) == 4);
	check(sizeof(double) == 8);
	check(GEngineNetVersion == 0 || GEngineNetVersion >= GEngineMinNetVersion || GEngineVersion.IsLicenseeVersion());

	// Init list of common colors.
	GColorList.CreateColorMap();

	bool bForceSmokeTests = false;
	GConfig->GetBool(TEXT("AutomationTesting"), TEXT("bForceSmokeTests"), bForceSmokeTests, GEngineIni);
	bForceSmokeTests |= FParse::Param(FCommandLine::Get(), TEXT("bForceSmokeTests"));
	FAutomationTestFramework::GetInstance().SetForceSmokeTests(bForceSmokeTests);

	// Init other systems.
	FCoreDelegates::OnInit.Broadcast();
	return true;
}


void FEngineLoop::AppPreExit( )
{
	UE_LOG(LogExit, Log, TEXT("Preparing to exit.") );

	FCoreDelegates::OnPreExit.Broadcast();

#if WITH_ENGINE
	if (FString(FCommandLine::Get()).Contains(TEXT("CreatePak")) && GetDerivedDataCache())
	{
		// if we are creating a Pak, we need to make sure everything is done and written before we exit
		UE_LOG(LogInit, Display, TEXT("Closing DDC Pak File."));
		GetDerivedDataCacheRef().WaitForQuiescence(true);
	}
#endif

#if WITH_EDITOR
	FRemoteConfig::Flush();
#endif

	FCoreDelegates::OnExit.Broadcast();

	// Clean up the thread pool
	if (GThreadPool != nullptr)
	{
		GThreadPool->Destroy();
	}

#if WITH_ENGINE
	if ( GShaderCompilingManager )
	{
		GShaderCompilingManager->Shutdown();

		delete GShaderCompilingManager;
		GShaderCompilingManager = nullptr;
	}
#endif
}


void FEngineLoop::AppExit( )
{
	UE_LOG(LogExit, Log, TEXT("Exiting."));

	FPlatformMisc::PlatformTearDown();

	if (GConfig)
	{
		GConfig->Exit();
		delete GConfig;
		GConfig = NULL;
	}

	if( GLog )
	{
		GLog->TearDown();
	}

	FInternationalization::TearDown();
}

void FEngineLoop::PreInitHMDDevice()
{
#if WITH_ENGINE && !UE_SERVER
	if (!GIsEditor)
	{
		//@todo vr: only preinit first valid hmd?
		if (!FParse::Param(FCommandLine::Get(), TEXT("nohmd")) && !FParse::Param(FCommandLine::Get(), TEXT("emulatestereo")))
		{
			// Get a list of plugins that implement this feature
			TArray<IHeadMountedDisplayModule*> HMDImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IHeadMountedDisplayModule>(IHeadMountedDisplayModule::GetModularFeatureName());
			for (auto HMDModuleIt = HMDImplementations.CreateIterator(); HMDModuleIt; ++HMDModuleIt)
			{
				(*HMDModuleIt)->PreInit();
			}
		}
	}
#endif // #if WITH_ENGINE && !UE_SERVER
}

#undef LOCTEXT_NAMESPACE