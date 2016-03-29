// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"
#include "GUI.h"
#include "P4DataCache.h"
#include "P4Env.h"
#include "XmlParser.h"
#include "SettingsCache.h"

#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealSync, Log, All);

IMPLEMENT_APPLICATION(UnrealSync, "UnrealSync");

bool FUnrealSync::UpdateOriginalUS(const FString& OriginalUSPath)
{
	FString Output;
	return FP4Env::RunP4Output(TEXT("sync ") + OriginalUSPath, Output);
}

bool FUnrealSync::RunDetachedUS(const FString& USPath, bool bDoNotRunFromCopy, bool bDoNotUpdateOnStartUp, bool bPassP4Env)
{
	FString CommandLine = FString()
		+ (bDoNotRunFromCopy ? TEXT("-DoNotRunFromCopy ") : TEXT(""))
		+ (bDoNotUpdateOnStartUp ? TEXT("-DoNotUpdateOnStartUp ") : TEXT(""))
		+ (FUnrealSync::IsDebugParameterSet() ? TEXT("-Debug ") : TEXT(""))
		+ (bPassP4Env ? *FP4Env::Get().GetCommandLine() : TEXT(""));

	return RunProcess(USPath, CommandLine);
}

bool FUnrealSync::DeleteIfExistsAndCopy(const FString& To, const FString& From)
{
	auto& PlatformPhysical = IPlatformFile::GetPlatformPhysical();

	if (PlatformPhysical.FileExists(*To) && !PlatformPhysical.DeleteFile(*To))
	{
		UE_LOG(LogUnrealSync, Warning, TEXT("Deleting existing file '%s' failed."), *To);
		return false;
	}
	else if (PlatformPhysical.DirectoryExists(*To) && !PlatformPhysical.DeleteDirectoryRecursively(*To))
	{
		return false;
	}

	if (PlatformPhysical.FileExists(*From))
	{
		return PlatformPhysical.CopyFile(*To, *From);
	}
	else if (PlatformPhysical.DirectoryExists(*From))
	{
		return PlatformPhysical.CopyDirectoryTree(*To, *From, true);
	}

	return false;
}

void FUnrealSync::DeleteStaleBinaries(const FOnSyncProgress& OnSyncProgress, bool bPreview)
{
	OnSyncProgress.Execute(TEXT("... obtaining P4 workspace have files.") LINE_TERMINATOR);

	FString Haves;
	FP4Env::RunP4Output(FString::Printf(TEXT("have %s/.../Binaries/..."), *FP4Env::Get().GetBranch()), Haves);

	FRegexPattern HaveFilePattern(TEXT("\\s+//.+#\\d+ - (.+)"));
	FRegexMatcher Matcher(HaveFilePattern, Haves);

	TArray<FString> HaveFiles;

	while (Matcher.FindNext())
	{
		FString HaveFile = Matcher.GetCaptureGroup(1);

		FPaths::NormalizeFilename(HaveFile);

#ifdef PLATFORM_WINDOWS
		HaveFiles.Add(HaveFile.ToLower());
#else
		HaveFiles.Add(HaveFile);
#endif
	}

	OnSyncProgress.Execute(TEXT("... searching file system and diffing with P4 have state.") LINE_TERMINATOR);

	IPlatformFile& FS = IPlatformFile::GetPlatformPhysical();

	FString BranchDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::EngineDir(), TEXT("..")));
	TArray<FString> ToDelete;
	struct FPotentialStaleBinariesVisitor : public IPlatformFile::FDirectoryVisitor
	{
		FPotentialStaleBinariesVisitor(const FString& InBranchDir, const TArray<FString>& InP4HaveFiles, TArray<FString>& OutToDelete)
			: BranchDir(InBranchDir), Output(OutToDelete), P4HaveFiles(InP4HaveFiles), FS(IPlatformFile::GetPlatformPhysical())
		{
			DirsBlacklist.Add(TEXT("Build"));
			DirsBlacklist.Add(TEXT("Config"));
			DirsBlacklist.Add(TEXT("Content"));
			DirsBlacklist.Add(TEXT("DerivedDataCache"));
			DirsBlacklist.Add(TEXT("Documentation"));
			DirsBlacklist.Add(TEXT("Intermediate"));
			DirsBlacklist.Add(TEXT("Saved"));
			DirsBlacklist.Add(TEXT("Shaders"));
			DirsBlacklist.Add(TEXT("Source"));
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{

#ifdef PLATFORM_WINDOWS
			const ESearchCase::Type CaseSensitiveness = ESearchCase::IgnoreCase;
#elif PLATFORM_LINUX || PLATFORM_MAC
			const ESearchCase::Type CaseSensitiveness = ESearchCase::CaseSensitive;
#endif

			if (!bIsDirectory)
			{
				FString File(FilenameOrDirectory);

				FString Filename = FPaths::GetBaseFilename(File);
				FString Directory = FPaths::GetPath(File);
				FString Extension = FPaths::GetExtension(File);

				if (Directory.Contains(TEXT("Binaries"), CaseSensitiveness) &&
					IsStaleBinaryExtension(Extension) &&
					(
						Filename.StartsWith(TEXT("UE4Editor")) ||
						Filename.StartsWith(TEXT("UE4Game"))
					))
				{
					FPaths::NormalizeFilename(File);
#ifdef PLATFORM_WINDOWS
					FString NormCaseFile = File.ToLower();
#else
					FString NormCaseFile = File;
#endif
					if (!P4HaveFiles.ContainsByPredicate([&NormCaseFile](const FString& Other)
						{
							return NormCaseFile.Equals(Other, ESearchCase::CaseSensitive);
						}))
					{
						Output.Add(MoveTemp(File));
					}
				}
			}
			else
			{
				FString DirName = FPaths::GetBaseFilename(FilenameOrDirectory);

				for (const auto& BlacklistedDirName : DirsBlacklist)
				{
					if (DirName.Equals(BlacklistedDirName, CaseSensitiveness))
					{
						return true;
					}
				}

				FS.IterateDirectory(FilenameOrDirectory, *this);
			}

			return true;
		}

	private:
		static bool IsStaleBinaryExtension(const FString& Extension)
		{
#ifdef PLATFORM_WINDOWS
			return Extension.Equals(TEXT("pdb"), ESearchCase::IgnoreCase) ||
				Extension.Equals(TEXT("exe"), ESearchCase::IgnoreCase) ||
				Extension.Equals(TEXT("dll"), ESearchCase::IgnoreCase);
#elif PLATFORM_LINUX
			return Extension.Equals(TEXT("")) ||
				Extension.Equals(TEXT("so"));
#elif PLATFORM_MAC
			return Extension.Equals(TEXT("")) ||
				Extension.Equals(TEXT("dSYM")) ||
				Extension.Equals(TEXT("dylib"));
#endif
		}

		const FString& BranchDir;
		TArray<FString>& Output;
		const TArray<FString>& P4HaveFiles;
		IPlatformFile& FS;

		TArray<FString> DirsBlacklist;
	} Visitor(BranchDir, HaveFiles, ToDelete);

	FS.IterateDirectory(*BranchDir, Visitor);

	if (bPreview)
	{
		for (const auto& FileToDelete : ToDelete)
		{
			OnSyncProgress.Execute(FString::Printf(TEXT("... would delete (preview) stale file %s") LINE_TERMINATOR, *FileToDelete));
		}
	}
	else
	{
		for (const auto& FileToDelete : ToDelete)
		{
			OnSyncProgress.Execute(FString::Printf(TEXT("... deleting stale file %s") LINE_TERMINATOR, *FileToDelete));
			if (!FS.DeleteFile(*FileToDelete))
			{
				OnSyncProgress.Execute(FString::Printf(TEXT("\t... failed to delete %s") LINE_TERMINATOR, *FileToDelete));
			}
		}
	}
}

void FUnrealSync::RunUnrealSync(const TCHAR* CommandLine)
{
	// start up the main loop
	GEngineLoop.PreInit(CommandLine);

	if (!FP4Env::Init(CommandLine))
	{
		GLog->Flush();
		InitGUI(CommandLine, true);
	}
	else
	{
		if (FUnrealSync::Initialization(CommandLine))
		{
			InitGUI(CommandLine);
		}

		if (!FUnrealSync::GetInitializationError().IsEmpty())
		{
			UE_LOG(LogUnrealSync, Fatal, TEXT("Error: %s"), *FUnrealSync::GetInitializationError());
		}
		else
		{
			GIsRequestingExit = true;
		}
	}
}

const FString& FUnrealSync::GetSharedPromotableDisplayName()
{
	static const FString DispName = "Shared Promotable";

	return DispName;
}

const FString& FUnrealSync::GetSharedPromotableP4FolderName()
{
	static const FString DispName = "Samples";

	return DispName;
}

/**
 * Helper function to send message through sync progress delegate.
 *
 * @param OnSyncProgress Delegate to use.
 * @param Message Message to send.
 */
void SyncingMessage(const FUnrealSync::FOnSyncProgress& OnSyncProgress, const FString& Message)
{
	if (OnSyncProgress.IsBound())
	{
		OnSyncProgress.Execute(Message + "\n");
	}
}

/**
 * Contains file and revision specification as sync step.
 */
struct FSyncStep
{
	FSyncStep(FString InFileSpec, FString InRevSpec)
		: FileSpec(MoveTemp(InFileSpec)), RevSpec(MoveTemp(InRevSpec))
	{}

	const FString& GetFileSpec() const { return FileSpec; }
	const FString& GetRevSpec() const { return RevSpec; }

private:
	FString FileSpec;
	FString RevSpec;
};

/**
 * Parses sync rules XML content and fills array with sync steps
 * appended with content and program revision specs.
 *
 * @param SyncSteps Array to append.
 * @param SyncRules String with sync rules XML.
 * @param ContentRevisionSpec Content revision spec.
 * @param ProgramRevisionSpec Program revision spec.
 */
void FillWithSyncSteps(TArray<FSyncStep>& SyncSteps, const FString& SyncRules, const FString& ContentRevisionSpec, const FString& ProgramRevisionSpec)
{
	TSharedRef<FXmlFile> Doc = MakeShareable(new FXmlFile(SyncRules, EConstructMethod::ConstructFromBuffer));

	for (FXmlNode* ChildNode : Doc->GetRootNode()->GetChildrenNodes())
	{
		if (ChildNode->GetTag() != "Rules")
		{
			continue;
		}

		for (FXmlNode* Rule : ChildNode->GetChildrenNodes())
		{
			if (Rule->GetTag() != "string")
			{
				continue;
			}

			auto SyncStep = Rule->GetContent();

			SyncStep = SyncStep.Replace(TEXT("%LABEL_TO_SYNC_TO%"), *ProgramRevisionSpec);

			// If there was no label in sync step.
			// TODO: This is hack for messy ArtistSyncRules.xml format.
			// Needs to be changed soon.
			if (!SyncStep.Contains("@"))
			{
				SyncStep += ContentRevisionSpec;
			}

			int32 AtLastPos = INDEX_NONE;
			SyncStep.FindLastChar('@', AtLastPos);

			int32 HashLastPos = INDEX_NONE;
			SyncStep.FindLastChar('#', HashLastPos);

			int32 RevSpecSeparatorPos = FMath::Max<int32>(AtLastPos, HashLastPos);

			check(RevSpecSeparatorPos != INDEX_NONE); // At least one rev specifier needed here.
			
			SyncSteps.Add(FSyncStep(SyncStep.Mid(0, RevSpecSeparatorPos), SyncStep.Mid(RevSpecSeparatorPos)));
		}
	}
}

/**
 * Gets latest P4 submitted changelist.
 *
 * @param CL Output parameter. Found changelist.
 *
 * @returns True if succeeded. False otherwise.
 */
static bool GetLatestChangelist(FString& CL)
{
	FString Left;
	FString Temp;
	FString Rest;

	if (!FP4Env::RunP4Output("changes -s submitted -m1", Temp)
		|| !Temp.Split(" on ", &Left, &Rest)
		|| !Left.Split("Change ", &Rest, &Temp))
	{
		return false;
	}

	CL = Temp;
	return true;
}

bool FUnrealSync::Sync(const FSyncSettings& Settings, const FString& Label, const FString& Game, const FOnSyncProgress& OnSyncProgress)
{
	SyncingMessage(OnSyncProgress, "Syncing to label: " + Label);

	auto ProgramRevisionSpec = "@" + Label;
	TArray<FSyncStep> SyncSteps;

	if (Settings.bArtist)
	{
		SyncingMessage(OnSyncProgress, "Performing artist sync.");
		
		// Get latest CL number to sync cause @head can change during
		// different syncs and it could create integrity problems in
		// workspace.
		FString CL;
		if (!GetLatestChangelist(CL))
		{
			return false;
		}

		SyncingMessage(OnSyncProgress, "Latest CL found: " + CL);

		auto ContentRevisionSpec = "@" + CL;

		auto ArtistSyncRulesPath = FString::Printf(TEXT("%s/%s/Build/ArtistSyncRules.xml"),
			*FP4Env::Get().GetBranch(), Game.IsEmpty() ? TEXT("Samples") : *Game);

		FString SyncRules;
		if (!FP4Env::RunP4Output("print -q " + ArtistSyncRulesPath + "#head", SyncRules) || SyncRules.IsEmpty())
		{
			return false;
		}

		SyncingMessage(OnSyncProgress, "Got sync rules from: " + ArtistSyncRulesPath);

		FillWithSyncSteps(SyncSteps, SyncRules, ContentRevisionSpec, ProgramRevisionSpec);
	}
	else
	{
		SyncSteps.Add(FSyncStep(Settings.OverrideSyncStep.IsEmpty() ? "/..." : Settings.OverrideSyncStep, ProgramRevisionSpec)); // all files to label
	}

	class FSyncCollectAndPassThrough
	{
	public:
		FSyncCollectAndPassThrough(const FOnSyncProgress& InProgress)
			: Progress(InProgress)
		{ }

		operator FOnSyncProgress() const
		{
			return FOnSyncProgress::CreateRaw(this, &FSyncCollectAndPassThrough::OnProgress);
		}

		bool OnProgress(const FString& Text)
		{
			Log += Text;

			return Progress.Execute(Text);
		}

		TArray<FString> GetCantClobbers() const
		{
			TArray<FString> CantClobbers;
			FString NormalizedLog = Log.Replace(TEXT("\r"), TEXT(""));
			const FRegexPattern CantClobber(TEXT("Can't clobber writable file ([^\\n]+)"));

			FRegexMatcher Match(CantClobber, NormalizedLog);

			while (Match.FindNext())
			{
				CantClobbers.Add(Match.GetCaptureGroup(1));
			}

			return CantClobbers;
		}

	private:
		const FOnSyncProgress& Progress;

		FString Log;
	};

	FString CommandPrefix = FString("sync ") + (Settings.bPreview ? "-n " : "") + FP4Env::Get().GetBranch();
	for(const auto& SyncStep : SyncSteps)
	{
		FSyncCollectAndPassThrough ErrorsCollector(OnSyncProgress);
		if (!FP4Env::RunP4Progress(CommandPrefix + SyncStep.GetFileSpec() + SyncStep.GetRevSpec(), ErrorsCollector))
		{
			if (!Settings.bAutoClobber)
			{
				return false;
			}

			FString AutoClobberCommandPrefix("sync -f ");
			for (const auto& CantClobber : ErrorsCollector.GetCantClobbers())
			{
				if (!FP4Env::RunP4Progress(AutoClobberCommandPrefix + CantClobber + SyncStep.GetRevSpec(), OnSyncProgress))
				{
					return false;
				}
			}
		}
	}

	if (Settings.bDeleteStaleBinaries)
	{
		OnSyncProgress.Execute(TEXT("Deleting stale binaries...") LINE_TERMINATOR);
		DeleteStaleBinaries(OnSyncProgress, Settings.bPreview);
		OnSyncProgress.Execute(TEXT("Stale binaries deleted.") LINE_TERMINATOR);
	}

	return true;
}

void FUnrealSync::SaveGUISettingsToCache()
{
	SaveGUISettings();
}

void FUnrealSync::SaveSettings()
{
	FSettingsCache::Get().Save();
}

void FUnrealSync::SaveSettingsAndClose()
{
	SaveGUISettingsToCache();

	SaveSettings();

	FPlatformMisc::RequestExit(false);
}

void FUnrealSync::SaveSettingsAndRestart()
{
	SaveGUISettingsToCache();

	SaveSettings();

	FUnrealSync::RunDetachedUS(FPlatformProcess::ExecutableName(false), true, true, false);
	FPlatformMisc::RequestExit(false);
}

bool FUnrealSync::IsDebugParameterSet()
{
	const auto bDebug = FParse::Param(FCommandLine::Get(), TEXT("Debug"));

	return bDebug;
}

bool FUnrealSync::Initialization(const TCHAR* CommandLine)
{
	const TCHAR* Platform =
#if PLATFORM_WINDOWS
	#if PLATFORM_64BITS
		TEXT("Win64")
	#else
		TEXT("Win32")
	#endif
#elif PLATFORM_MAC
		TEXT("Mac")
#elif PLATFORM_LINUX
		TEXT("Linux")
#endif
		;

	FString CommonExecutablePath =
		FPaths::ConvertRelativePathToFull(
		FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries"), Platform,
#if !UE_BUILD_DEBUG
		TEXT("UnrealSync")
#else
		*FString::Printf(TEXT("UnrealSync-%s-Debug"), Platform)
#endif
		));

	const TCHAR* AppPostfix =
#if PLATFORM_WINDOWS
		TEXT(".exe")
#elif PLATFORM_MAC
		TEXT(".app")
#elif PLATFORM_LINUX
		TEXT("")
#endif
		;

	FString OriginalExecutablePath = CommonExecutablePath + AppPostfix;
	FString TemporaryExecutablePath = CommonExecutablePath + TEXT(".Temporary") + AppPostfix;

	bool bDoNotRunFromCopy = FParse::Param(CommandLine, TEXT("DoNotRunFromCopy"));
	bool bDoNotUpdateOnStartUp = FParse::Param(CommandLine, TEXT("DoNotUpdateOnStartUp"));

	if (!bDoNotRunFromCopy)
	{
		if (!DeleteIfExistsAndCopy(*TemporaryExecutablePath, *OriginalExecutablePath))
		{
			InitializationError = TEXT("Copying UnrealSync to temp location failed.");
		}
		else if (!RunDetachedUS(TemporaryExecutablePath, true, bDoNotUpdateOnStartUp, true))
		{
			InitializationError = TEXT("Running remote UnrealSync failed.");
		}

		return false;
	}

	if (!bDoNotUpdateOnStartUp && FP4Env::CheckIfFileNeedsUpdate(OriginalExecutablePath))
	{
		if (!UpdateOriginalUS(OriginalExecutablePath))
		{
			InitializationError = TEXT("UnrealSync update failed.");
		}
		else if (!RunDetachedUS(OriginalExecutablePath, false, true, true))
		{
			InitializationError = TEXT("Running UnrealSync failed.");
		}

		return false;
	}

	return true;
}


/* Static fields initialization. */
FString FUnrealSync::InitializationError;
