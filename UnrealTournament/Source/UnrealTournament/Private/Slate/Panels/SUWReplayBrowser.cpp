// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "SUWReplayBrowser.h"
#include "Net/UnrealNetwork.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUWReplayBrowser::ConstructPanel(FVector2D ViewportSize)
{
	Tag = FName(TEXT("ReplayBrowser"));

	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	}

	bShouldShowAllReplays = false;

	this->ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.Backdrop"))
				]
			]
		]
		
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(25.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(WatchReplayButton, SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BlankButton")
				.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))

				.Text(NSLOCTEXT("SUWReplayBrowser", "WatchReplay", "Watch A Replay"))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.OnClicked(this, &SUWReplayBrowser::OnWatchClick)
			]
		]
	];

	ReplayStreamer = FNetworkReplayStreaming::Get().GetFactory().CreateReplayStreamer();

	BuildReplayList();
}

void SUWReplayBrowser::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUWPanel::OnShowPanel(inParentWindow);
	/*
	if (bNeedsRefresh)
	{
		BuildReplayList();
	}
	*/
}

void SUWReplayBrowser::BuildReplayList()
{
	ReplayList.Empty();

	if (ReplayStreamer.IsValid())
	{
		FNetworkReplayVersion Version = FNetworkVersion::GetReplayVersion();
		if (bShouldShowAllReplays)
		{
			Version.NetworkVersion = 0;
			Version.Changelist = 0;
		}

		const FString MetaString = TEXT("");

		ReplayStreamer->EnumerateStreams(Version, FOnEnumerateStreamsComplete::CreateSP(this, &SUWReplayBrowser::OnEnumerateStreamsComplete));
	}
}

FReply SUWReplayBrowser::OnWatchClick()
{
	if (ReplayList.Num() > 0)
	{
		if (PlayerOwner.IsValid() && PlayerOwner->GetWorld())
		{
			GEngine->Exec(PlayerOwner->GetWorld(), *FString::Printf(TEXT("DEMOPLAY %s"), *ReplayList[0]->StreamInfo.Name));
		}
	}

	return FReply::Handled();
}

void SUWReplayBrowser::OnEnumerateStreamsComplete(const TArray<FNetworkReplayStreamInfo>& Streams)
{
	for (const auto& StreamInfo : Streams)
	{
		float SizeInKilobytes = StreamInfo.SizeInBytes / 1024.0f;

		TSharedPtr<FReplayEntry> NewDemoEntry = MakeShareable(new FReplayEntry());

		NewDemoEntry->StreamInfo = StreamInfo;
		NewDemoEntry->Date = StreamInfo.Timestamp.ToString(TEXT("%m/%d/%Y %h:%M %A"));	// UTC time
		NewDemoEntry->Size = SizeInKilobytes >= 1024.0f ? FString::Printf(TEXT("%2.2f MB"), SizeInKilobytes / 1024.0f) : FString::Printf(TEXT("%i KB"), (int)SizeInKilobytes);

		UE_LOG(UT, Log, TEXT("Stream found %s, Live: %s"), *StreamInfo.FriendlyName, StreamInfo.bIsLive ? TEXT("YES") : TEXT("NO"));

		ReplayList.Add(NewDemoEntry);
	}

	// Sort demo names by date
	struct FCompareDateTime
	{
		FORCEINLINE bool operator()(const TSharedPtr<FReplayEntry> & A, const TSharedPtr<FReplayEntry> & B) const
		{
			if (A->StreamInfo.bIsLive != B->StreamInfo.bIsLive)
			{
				return A->StreamInfo.bIsLive;
			}

			return A->StreamInfo.Timestamp.GetTicks() > B->StreamInfo.Timestamp.GetTicks();
		}
	};

	Sort(ReplayList.GetData(), ReplayList.Num(), FCompareDateTime());
}

#endif