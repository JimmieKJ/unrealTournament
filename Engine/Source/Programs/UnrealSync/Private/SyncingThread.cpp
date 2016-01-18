// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"

#include "SyncingThread.h"

FSyncingThread::FSyncingThread(FSyncSettings InSettings, ILabelNameProvider& InLabelNameProvider, const FOnSyncFinished& InOnSyncFinished, const FOnSyncProgress& InOnSyncProgress)
	: bTerminate(false), Settings(MoveTemp(InSettings)), LabelNameProvider(InLabelNameProvider), OnSyncFinished(InOnSyncFinished), OnSyncProgress(InOnSyncProgress)
{
	Thread = FRunnableThread::Create(this, TEXT("Syncing thread"));
}

uint32 FSyncingThread::Run()
{
	if (OnSyncProgress.IsBound())
	{
		OnSyncProgress.Execute(LabelNameProvider.GetStartedMessage() + "\n");
	}

	FString Label = LabelNameProvider.GetLabelName();
	FString Game = LabelNameProvider.GetGameName();

	struct FProcessStopper
	{
		FProcessStopper(bool& bInStop, FOnSyncProgress& InOuterSyncProgress)
			: bStop(bInStop), OuterSyncProgress(InOuterSyncProgress) {}

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
		FOnSyncProgress& OuterSyncProgress;
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

void FSyncingThread::Terminate()
{
	bTerminate = true;
	Thread->WaitForCompletion();
}
