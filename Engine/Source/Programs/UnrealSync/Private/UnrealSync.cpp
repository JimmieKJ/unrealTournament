// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"
#include "GUI.h"
#include "P4DataCache.h"
#include "P4Env.h"
#include "XmlParser.h"

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

bool FUnrealSync::DeleteIfExistsAndCopyFile(const FString& To, const FString& From)
{
	auto& PlatformPhysical = IPlatformFile::GetPlatformPhysical();

	if (PlatformPhysical.FileExists(*To) && !PlatformPhysical.DeleteFile(*To))
	{
		UE_LOG(LogUnrealSync, Warning, TEXT("Deleting existing file '%s' failed."), *To);
		return false;
	}

	return PlatformPhysical.CopyFile(*To, *From);
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
	}
}

FString FUnrealSync::GetLatestLabelForGame(const FString& GameName)
{
	auto Labels = GetPromotedLabelsForGame(GameName);
	return Labels->Num() == 0 ? "" : (*Labels)[0];
}

/**
 * Tells if label is in the format: <BranchPath>/<Prefix>-<GameNameIfNotEmpty>-CL-*
 *
 * @param LabelName Label name to check.
 * @param LabelNamePrefix Prefix to use.
 * @param GameName Game name to use.
 *
 * @returns True if label is in the format. False otherwise.
 */
bool IsPrefixedGameLabelName(const FString& LabelName, const FString& LabelNamePrefix, const FString& GameName)
{
	int32 BranchNameEnd = 0;

	if (!LabelName.FindLastChar('/', BranchNameEnd))
	{
		return false;
	}

	FString Rest = LabelName.Mid(BranchNameEnd + 1);

	FString LabelNameGamePart;
	if (!GameName.Equals(""))
	{
		LabelNameGamePart = "-" + GameName;
	}

	return Rest.StartsWith(LabelNamePrefix + LabelNameGamePart + "-CL-", ESearchCase::CaseSensitive);
}

/**
 * Tells if label is in the format: <BranchPath>/Promoted-<GameNameIfNotEmpty>-CL-*
 *
 * @param LabelName Label name to check.
 * @param GameName Game name to use.
 *
 * @returns True if label is in the format. False otherwise.
 */
bool IsPromotedGameLabelName(const FString& LabelName, const FString& GameName)
{
	return IsPrefixedGameLabelName(LabelName, "Promoted", GameName);
}

/**
 * Tells if label is in the format: <BranchPath>/Promotable-<GameNameIfNotEmpty>-CL-*
 *
 * @param LabelName Label name to check.
 * @param GameName Game name to use.
 *
 * @returns True if label is in the format. False otherwise.
 */
bool IsPromotableGameLabelName(const FString& LabelName, const FString& GameName)
{
	return IsPrefixedGameLabelName(LabelName, "Promotable", GameName);
}

/**
 * Helper function to construct and get array of labels if data is fetched from the P4
 * using given policy.
 *
 * @template_param TFillLabelsPolicy Policy class that has to implement:
 *     static void Fill(TArray<FString>& OutLabelNames, const FString& GameName, const TArray<FP4Label>& Labels)
 *     that will fill the array on request.
 * @param GameName Game name to pass to the Fill method from policy.
 * @returns Filled out labels.
 */
template <class TFillLabelsPolicy>
TSharedPtr<TArray<FString> > GetLabelsForGame(const FString& GameName)
{
	TSharedPtr<TArray<FString> > OutputLabels = MakeShareable(new TArray<FString>());

	if (FUnrealSync::HasValidData())
	{
		TFillLabelsPolicy::Fill(*OutputLabels, GameName, FUnrealSync::GetLabels());
	}

	return OutputLabels;
}

TSharedPtr<TArray<FString> > FUnrealSync::GetPromotedLabelsForGame(const FString& GameName)
{
	struct TFillLabelsPolicy 
	{
		static void Fill(TArray<FString>& OutLabelNames, const FString& GameName, const TArray<FP4Label>& Labels)
		{
			for (auto Label : Labels)
			{
				if (IsPromotedGameLabelName(Label.GetName(), GameName))
				{
					OutLabelNames.Add(Label.GetName());
				}
			}
		}
	};

	return GetLabelsForGame<TFillLabelsPolicy>(GameName);
}

TSharedPtr<TArray<FString> > FUnrealSync::GetPromotableLabelsForGame(const FString& GameName)
{
	struct TFillLabelsPolicy
	{
		static void Fill(TArray<FString>& OutLabelNames, const FString& GameName, const TArray<FP4Label>& Labels)
		{
			for (auto Label : Labels)
			{
				if (IsPromotedGameLabelName(Label.GetName(), GameName))
				{
					break;
				}

				if (IsPromotableGameLabelName(Label.GetName(), GameName))
				{
					OutLabelNames.Add(Label.GetName());
				}
			}
		}
	};

	return GetLabelsForGame<TFillLabelsPolicy>(GameName);
}

TSharedPtr<TArray<FString> > FUnrealSync::GetAllLabels()
{
	struct TFillLabelsPolicy
	{
		static void Fill(TArray<FString>& OutLabelNames, const FString& GameName, const TArray<FP4Label>& Labels)
		{
			for (auto Label : Labels)
			{
				OutLabelNames.Add(Label.GetName());
			}
		}
	};

	return GetLabelsForGame<TFillLabelsPolicy>("");
}

TSharedPtr<TArray<FString> > FUnrealSync::GetPossibleGameNames()
{
	TSharedPtr<TArray<FString> > PossibleGames = MakeShareable(new TArray<FString>());

	FP4Env& Env = FP4Env::Get();

	FString FileList;
	if (!Env.RunP4Output(FString::Printf(TEXT("files -e %s/.../Build/ArtistSyncRules.xml"), *Env.GetBranch()), FileList) || FileList.IsEmpty())
	{
		return PossibleGames;
	}

	FString Line, Rest = FileList;
	while (Rest.Split(LINE_TERMINATOR, &Line, &Rest, ESearchCase::CaseSensitive))
	{
		if (!Line.StartsWith(Env.GetBranch()))
		{
			continue;
		}

		int32 ArtistSyncRulesPos = Line.Find("/Build/ArtistSyncRules.xml#", ESearchCase::IgnoreCase);

		if (ArtistSyncRulesPos == INDEX_NONE)
		{
			continue;
		}

		FString MiddlePart = Line.Mid(Env.GetBranch().Len(), ArtistSyncRulesPos - Env.GetBranch().Len());

		int32 LastSlash = INDEX_NONE;
		MiddlePart.FindLastChar('/', LastSlash);

		PossibleGames->Add(MiddlePart.Mid(LastSlash + 1));
	}

	return PossibleGames;
}

const FString& FUnrealSync::GetSharedPromotableDisplayName()
{
	static const FString DispName = "Shared Promotable";

	return DispName;
}

void FUnrealSync::RegisterOnDataLoaded(const FOnDataLoaded& OnDataLoaded)
{
	FUnrealSync::OnDataLoaded = OnDataLoaded;
}

void FUnrealSync::RegisterOnDataReset(const FOnDataReset& OnDataReset)
{
	FUnrealSync::OnDataReset = OnDataReset;
}

void FUnrealSync::StartLoadingData()
{
	bLoadingFinished = false;
	Data.Reset();
	LoaderThread.Reset();

	OnDataReset.ExecuteIfBound();

	LoaderThread = MakeShareable(new FP4DataLoader(
		FP4DataLoader::FOnLoadingFinished::CreateStatic(&FUnrealSync::OnP4DataLoadingFinished)
		));
}

void FUnrealSync::TerminateLoadingProcess()
{
	if (LoaderThread.IsValid())
	{
		LoaderThread->Terminate();
	}
}

void FUnrealSync::OnP4DataLoadingFinished(TSharedPtr<FP4DataCache> Data)
{
	FUnrealSync::Data = Data;

	bLoadingFinished = true;

	OnDataLoaded.ExecuteIfBound();
}

bool FUnrealSync::LoadingFinished()
{
	return bLoadingFinished;
}

bool FUnrealSync::HasValidData()
{
	return Data.IsValid();
}

/**
 * Class to store info of syncing thread.
 */
class FSyncingThread : public FRunnable
{
public:
	/**
	 * Constructor
	 *
	 * @param Settings Sync settings.
	 * @param LabelNameProvider Label name provider.
	 * @param OnSyncFinished Delegate to run when syncing process has finished.
	 * @param OnSyncProgress Delegate to run when syncing process has made progress.
	 */
	FSyncingThread(FSyncSettings Settings, ILabelNameProvider& LabelNameProvider, const FUnrealSync::FOnSyncFinished& OnSyncFinished, const FUnrealSync::FOnSyncProgress& OnSyncProgress)
		: Settings(MoveTemp(Settings)), LabelNameProvider(LabelNameProvider), OnSyncFinished(OnSyncFinished), OnSyncProgress(OnSyncProgress), bTerminate(false)
	{
		Thread = FRunnableThread::Create(this, TEXT("Syncing thread"));
	}

	/**
	 * Main thread function.
	 */
	uint32 Run() override
	{
		if (OnSyncProgress.IsBound())
		{
			OnSyncProgress.Execute(LabelNameProvider.GetStartedMessage() + "\n");
		}

		FString Label = LabelNameProvider.GetLabelName();
		FString Game = LabelNameProvider.GetGameName();

		struct FProcessStopper
		{
			FProcessStopper(bool& bStop, FUnrealSync::FOnSyncProgress& OuterSyncProgress)
				: bStop(bStop), OuterSyncProgress(OuterSyncProgress) {}

			bool OnProgress(const FString& Text)
			{
				if (OuterSyncProgress.IsBound())
				{
					if (!OuterSyncProgress.Execute(Text))
					{
						bStop = true;
					}
				}

				return !bStop;
			}

		private:
			bool& bStop;
			FUnrealSync::FOnSyncProgress& OuterSyncProgress;
		};
		
		FProcessStopper Stopper(bTerminate, OnSyncProgress);
		bool bSuccess = FUnrealSync::Sync(Settings, Label, Game, FUnrealSync::FOnSyncProgress::CreateRaw(&Stopper, &FProcessStopper::OnProgress));

		if (OnSyncProgress.IsBound())
		{
			OnSyncProgress.Execute(LabelNameProvider.GetFinishedMessage() + "\n");
		}

		OnSyncFinished.ExecuteIfBound(bSuccess);

		return 0;
	}

	/**
	 * Stops process runnning in the background and terminates wait for the
	 * watcher thread to finish.
	 */
	void Terminate()
	{
		bTerminate = true;
		Thread->WaitForCompletion();
	}

private:
	/* Tells the thread to terminate the process. */
	bool bTerminate;

	/* Handle for thread object. */
	FRunnableThread* Thread;

	/* Sync settings. */
	FSyncSettings Settings;

	/* Label name provider. */
	ILabelNameProvider& LabelNameProvider;

	/* Delegate that will be run when syncing process has finished. */
	FUnrealSync::FOnSyncFinished OnSyncFinished;

	/* Delegate that will be run when syncing process has finished. */
	FUnrealSync::FOnSyncProgress OnSyncProgress;
};

void FUnrealSync::TerminateSyncingProcess()
{
	if (SyncingThread.IsValid())
	{
		SyncingThread->Terminate();
	}
}

void FUnrealSync::LaunchSync(FSyncSettings Settings, ILabelNameProvider& LabelNameProvider, const FOnSyncFinished& OnSyncFinished, const FOnSyncProgress& OnSyncProgress)
{
	SyncingThread = MakeShareable(new FSyncingThread(MoveTemp(Settings), LabelNameProvider, OnSyncFinished, OnSyncProgress));
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
	FSyncStep(FString FileSpec, FString RevSpec)
		: FileSpec(MoveTemp(FileSpec)), RevSpec(MoveTemp(RevSpec))
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
		FSyncCollectAndPassThrough(const FOnSyncProgress& Progress)
			: Progress(Progress)
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

		void ProcessLog()
		{
			Log = Log.Replace(TEXT("\r"), TEXT(""));
			const FRegexPattern CantClobber(TEXT("Can't clobber writable file ([^\\n]+)"));

			FRegexMatcher Match(CantClobber, Log);

			while (Match.FindNext())
			{
				CantClobbers.Add(Log.Mid(
					Match.GetCaptureGroupBeginning(1),
					Match.GetCaptureGroupEnding(1) - Match.GetCaptureGroupBeginning(1)));
			}
		}

		const TArray<FString>& GetCantClobbers() const { return CantClobbers; }

	private:
		const FOnSyncProgress& Progress;

		FString Log;

		TArray<FString> CantClobbers;
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

			ErrorsCollector.ProcessLog();

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

	return true;
}

bool FUnrealSync::IsDebugParameterSet()
{
	const auto bDebug = FParse::Param(FCommandLine::Get(), TEXT("Debug"));

	return bDebug;
}

bool FUnrealSync::Initialization(const TCHAR* CommandLine)
{
	FString CommonExecutablePath =
		FPaths::ConvertRelativePathToFull(
		FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries"), TEXT("Win64"),
#if !UE_BUILD_DEBUG
		TEXT("UnrealSync")
#else
		TEXT("UnrealSync-Win64-Debug")
#endif
		));

	FString OriginalExecutablePath = CommonExecutablePath + TEXT(".exe");
	FString TemporaryExecutablePath = CommonExecutablePath + TEXT(".Temporary.exe");

	bool bDoNotRunFromCopy = FParse::Param(CommandLine, TEXT("DoNotRunFromCopy"));
	bool bDoNotUpdateOnStartUp = FParse::Param(CommandLine, TEXT("DoNotUpdateOnStartUp"));

	if (!bDoNotRunFromCopy)
	{
		if (!DeleteIfExistsAndCopyFile(*TemporaryExecutablePath, *OriginalExecutablePath))
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
FUnrealSync::FOnDataLoaded FUnrealSync::OnDataLoaded;
FUnrealSync::FOnDataReset FUnrealSync::OnDataReset;
TSharedPtr<FP4DataCache> FUnrealSync::Data;
bool FUnrealSync::bLoadingFinished = false;
TSharedPtr<FP4DataLoader> FUnrealSync::LoaderThread;
TSharedPtr<FSyncingThread> FUnrealSync::SyncingThread;
FString FUnrealSync::InitializationError;