// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"

#include "P4DataCache.h"

#include "P4Env.h"
#include "Internationalization/Regex.h"

bool FP4DataCache::LoadFromLog(const FString& UnrealSyncListLog)
{
	const FRegexPattern Pattern(TEXT("Label ([^ ]+) (\\d{4})/(\\d{2})/(\\d{2}) (\\d{2}):(\\d{2}):(\\d{2})")); // '.+\\n

	FRegexMatcher Matcher(Pattern, UnrealSyncListLog);

	while (Matcher.FindNext())
	{
		Labels.Add(FP4Label(
			Matcher.GetCaptureGroup(1),
			FDateTime(
				FCString::Atoi(*Matcher.GetCaptureGroup(2)),
				FCString::Atoi(*Matcher.GetCaptureGroup(3)),
				FCString::Atoi(*Matcher.GetCaptureGroup(4)),
				FCString::Atoi(*Matcher.GetCaptureGroup(5)),
				FCString::Atoi(*Matcher.GetCaptureGroup(6)),
				FCString::Atoi(*Matcher.GetCaptureGroup(7))
			)));
	}

	Labels.Sort(
		[](const FP4Label& LabelA, const FP4Label& LabelB)
		{
			return LabelA.GetDate() > LabelB.GetDate();
		});

	return true;
}

const TArray<FP4Label>& FP4DataCache::GetLabels()
{
	return Labels;
}

FP4Label::FP4Label(const FString& InName, const FDateTime& InDate)
	: Name(InName), Date(InDate)
{

}

const FString& FP4Label::GetName() const
{
	return Name;
}

const FDateTime& FP4Label::GetDate() const
{
	return Date;
}

FP4DataLoader::FP4DataLoader(const FOnLoadingFinished& InOnLoadingFinished, FP4DataProxy& InDataProxy)
	: OnLoadingFinished(InOnLoadingFinished), DataProxy(InDataProxy), bTerminate(false)
{
	Thread = FRunnableThread::Create(this, TEXT("P4 Data Loading"));
}

uint32 FP4DataLoader::Run()
{
	TSharedPtr<FP4DataCache> Data = MakeShareable(new FP4DataCache());

	class P4Progress
	{
	public:
		P4Progress(const bool& bInTerminate)
			: bTerminate(bInTerminate)
		{
		}

		bool OnProgress(const FString& Input)
		{
			Output += Input;

			return !bTerminate;
		}

		FString Output;

	private:
		const bool &bTerminate;
	};

	P4Progress Progress(bTerminate);
	if (!FP4Env::RunP4Progress(FString::Printf(TEXT("labels -t -e%s/*"), *FP4Env::Get().GetBranch()),
		FP4Env::FOnP4MadeProgress::CreateRaw(&Progress, &P4Progress::OnProgress)))
	{
		OnLoadingFinished.ExecuteIfBound();
		return 0;
	}

	if (!Data->LoadFromLog(Progress.Output))
	{
		OnLoadingFinished.ExecuteIfBound();
	}

	DataProxy.OnP4DataLoadingFinished(Data);
	OnLoadingFinished.ExecuteIfBound();

	return 0;
}

void FP4DataLoader::Terminate()
{
	bTerminate = true;
	Thread->WaitForCompletion();
}

FP4DataLoader::~FP4DataLoader()
{
	Terminate();
}

bool FP4DataLoader::IsInProgress() const
{
	return bInProgress;
}

void FP4DataLoader::Exit()
{
	bInProgress = false;
}

bool FP4DataLoader::Init()
{
	bInProgress = true;

	return true;
}

FString FP4DataProxy::GetLatestLabelForGame(const FString& GameName)
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

TSharedPtr<TArray<FString> > FP4DataProxy::GetPromotedLabelsForGame(const FString& GameName)
{
	struct TFillLabelsPolicy 
	{
		static void Fill(TArray<FString>& OutLabelNames, const FString& InGameName, const TArray<FP4Label>& InLabels)
		{
			for (auto Label : InLabels)
			{
				if (IsPromotedGameLabelName(Label.GetName(), InGameName))
				{
					OutLabelNames.Add(Label.GetName());
				}
			}
		}
	};

	return GetLabelsForGame<TFillLabelsPolicy>(GameName);
}

TSharedPtr<TArray<FString> > FP4DataProxy::GetPromotableLabelsForGame(const FString& GameName)
{
	struct TFillLabelsPolicy
	{
		static void Fill(TArray<FString>& OutLabelNames, const FString& InGameName, const TArray<FP4Label>& InLabels)
		{
			for (auto Label : InLabels)
			{
				if (IsPromotedGameLabelName(Label.GetName(), InGameName))
				{
					break;
				}

				if (IsPromotableGameLabelName(Label.GetName(), InGameName))
				{
					OutLabelNames.Add(Label.GetName());
				}
			}
		}
	};

	return GetLabelsForGame<TFillLabelsPolicy>(GameName);
}

TSharedPtr<TArray<FString> > FP4DataProxy::GetAllLabels()
{
	struct TFillLabelsPolicy
	{
		static void Fill(TArray<FString>& OutLabelNames, const FString& InGameName, const TArray<FP4Label>& InLabels)
		{
			for (auto Label : InLabels)
			{
				OutLabelNames.Add(Label.GetName());
			}
		}
	};

	return GetLabelsForGame<TFillLabelsPolicy>("");
}

TSharedPtr<TArray<FString> > FP4DataProxy::GetPossibleGameNames()
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

void FP4DataProxy::OnP4DataLoadingFinished(TSharedPtr<FP4DataCache> InData)
{
	Data = InData;
}

bool FP4DataProxy::HasValidData() const
{
	return Data.IsValid();
}