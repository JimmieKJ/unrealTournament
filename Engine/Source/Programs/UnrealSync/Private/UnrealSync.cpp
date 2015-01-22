// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"
#include "GUI.h"
#include "P4DataCache.h"
#include "P4Env.h"

#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealSync, Log, All);

IMPLEMENT_APPLICATION(UnrealSync, "UnrealSync");

/**
 * Unreal sync main function.
 *
 * @param CommandLine Command line that the program was called with.
 */
void RunUnrealSync(const TCHAR* CommandLine)
{
	InitGUI(CommandLine);
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
	/* TODO: Hard coded game names. Needs to be fixed! */
	TSharedPtr<TArray<FString> > PossibleGames = MakeShareable(new TArray<FString>());

	PossibleGames->Add("FortniteGame");
	PossibleGames->Add("OrionGame");
	PossibleGames->Add("Shadow");
	PossibleGames->Add("Soul");

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
	 * @param bArtist Artist sync?
	 * @param bPreview Preview sync?
	 * @param LabelNameProvider Label name provider.
	 * @param OnSyncFinished Delegate to run when syncing process has finished.
	 * @param OnSyncProgress Delegate to run when syncing process has made progress.
	 */
	FSyncingThread(bool bArtist, bool bPreview, ILabelNameProvider& LabelNameProvider, const FUnrealSync::FOnSyncFinished& OnSyncFinished, const FUnrealSync::FOnSyncProgress& OnSyncProgress)
		: bArtist(bArtist), bPreview(bPreview), LabelNameProvider(LabelNameProvider), OnSyncFinished(OnSyncFinished), OnSyncProgress(OnSyncProgress)
	{
		FRunnableThread::Create(this, TEXT("Syncing thread"));
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
		
		bool bSuccess = FUnrealSync::Sync(bArtist, bPreview, Label, Game, OnSyncProgress);

		if (OnSyncProgress.IsBound())
		{
			OnSyncProgress.Execute(LabelNameProvider.GetFinishedMessage() + "\n");
		}

		OnSyncFinished.ExecuteIfBound(bSuccess);

		return 0;
	}

private:
	/* Artist sync? */
	bool bArtist;

	/* Preview sync? */
	bool bPreview;

	/* Label name provider. */
	ILabelNameProvider& LabelNameProvider;

	/* Delegate that will be run when syncing process has finished. */
	FUnrealSync::FOnSyncFinished OnSyncFinished;

	/* Delegate that will be run when syncing process has finished. */
	FUnrealSync::FOnSyncProgress OnSyncProgress;
};

void FUnrealSync::LaunchSync(bool bArtist, bool bPreview, ILabelNameProvider& LabelNameProvider, const FOnSyncFinished& OnSyncFinished, const FOnSyncProgress& OnSyncProgress)
{
	SyncingThread = MakeShareable(new FSyncingThread(bArtist, bPreview, LabelNameProvider, OnSyncFinished, OnSyncProgress));
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

#include "XmlParser.h"

/**
 * Parses sync rules XML content and fills array with sync steps
 * appended with content and program revision specs.
 *
 * @param SyncSteps Array to append.
 * @param SyncRules String with sync rules XML.
 * @param ContentRevisionSpec Content revision spec.
 * @param ProgramRevisionSpec Program revision spec.
 */
void FillWithSyncSteps(TArray<FString>& SyncSteps, const FString& SyncRules, const FString& ContentRevisionSpec, const FString& ProgramRevisionSpec)
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

			SyncSteps.Add(SyncStep);
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

bool FUnrealSync::Sync(bool bArtist, bool bPreview, const FString& Label, const FString& Game, const FOnSyncProgress& OnSyncProgress)
{
	SyncingMessage(OnSyncProgress, "Syncing to label: " + Label);

	auto ProgramRevisionSpec = "@" + Label;
	TArray<FString> SyncSteps;

	if (bArtist)
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
		if (!FP4Env::RunP4Output("print -q " + ArtistSyncRulesPath + "#head", SyncRules))
		{
			return false;
		}

		SyncingMessage(OnSyncProgress, "Got sync rules from: " + ArtistSyncRulesPath);

		FillWithSyncSteps(SyncSteps, SyncRules, ContentRevisionSpec, ProgramRevisionSpec);
	}
	else
	{
		SyncSteps.Add("/..." + ProgramRevisionSpec); // all files to label
	}

	for(auto SyncStep : SyncSteps)
	{
		if (!FP4Env::RunP4Progress(FString("sync ") + (bPreview ? "-n " : "") + FP4Env::Get().GetBranch() + SyncStep, OnSyncProgress))
		{
			return false;
		}
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