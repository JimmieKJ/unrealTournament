// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"

#include "P4DataCache.h"

#include "P4Env.h"
#include "Internationalization/Regex.h"

/**
 * Helper function to get capture group value.
 * Missing in FRegexMatcher implementation (maybe it should go there?).
 *
 * @param Source Source text that was used to match.
 * @param Matcher Current matcher state.
 * @param Id Capture group ID to retrieve.
 *
 * @returns Captured group text.
 */
FString GetGroupText(const FString& Source, FRegexMatcher& Matcher, int32 Id)
{
	return Source.Mid(Matcher.GetCaptureGroupBeginning(Id), Matcher.GetCaptureGroupEnding(Id) - Matcher.GetCaptureGroupBeginning(Id));
}

bool FP4DataCache::LoadFromLog(const FString& UnrealSyncListLog)
{
	const FRegexPattern Pattern(TEXT("Label ([^ ]+) (\\d{4})/(\\d{2})/(\\d{2}) (\\d{2}):(\\d{2}):(\\d{2})")); // '.+\\n

	FRegexMatcher Matcher(Pattern, UnrealSyncListLog);

	while (Matcher.FindNext())
	{
		Labels.Add(FP4Label(
			GetGroupText(UnrealSyncListLog, Matcher, 1),
			FDateTime(
				FCString::Atoi(*GetGroupText(UnrealSyncListLog, Matcher, 2)),
				FCString::Atoi(*GetGroupText(UnrealSyncListLog, Matcher, 3)),
				FCString::Atoi(*GetGroupText(UnrealSyncListLog, Matcher, 4)),
				FCString::Atoi(*GetGroupText(UnrealSyncListLog, Matcher, 5)),
				FCString::Atoi(*GetGroupText(UnrealSyncListLog, Matcher, 6)),
				FCString::Atoi(*GetGroupText(UnrealSyncListLog, Matcher, 7))
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

FP4Label::FP4Label(const FString& Name, const FDateTime& Date)
	: Name(Name), Date(Date)
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

FP4DataLoader::FP4DataLoader(const FOnLoadingFinished& OnLoadingFinished)
	: OnLoadingFinished(OnLoadingFinished), bTerminate(false)
{
	Thread = FRunnableThread::Create(this, TEXT("P4 Data Loading"));
}

uint32 FP4DataLoader::Run()
{
	TSharedPtr<FP4DataCache> Data = MakeShareable(new FP4DataCache());

	class P4Progress
	{
	public:
		P4Progress(const bool& bTerminate)
			: bTerminate(bTerminate)
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
		OnLoadingFinished.ExecuteIfBound(nullptr);
		return 0;
	}

	if (!Data->LoadFromLog(Progress.Output))
	{
		OnLoadingFinished.ExecuteIfBound(nullptr);
	}

	OnLoadingFinished.ExecuteIfBound(Data);

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
