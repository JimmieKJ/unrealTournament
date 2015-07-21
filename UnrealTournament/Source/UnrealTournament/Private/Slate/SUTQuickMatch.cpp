// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUTQuickMatch.h"
#include "SUWindowsStyle.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "OnlineSubsystemTypes.h"
#include "UTGameEngine.h"
#include "UTServerBeaconClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "UnrealNetwork.h"

#if !UE_SERVER

void SUTQuickMatch::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	QuickMatchType = InArgs._QuickMatchType;
	bWaitingForMatch = false;
	checkSlow(PlayerOwner != NULL);

	StartTime = PlayerOwner->GetWorld()->GetTimeSeconds();

	FVector2D ViewportSize;
	PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
	float DPIScale = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
	FVector2D DesignedRez(1920,1080);
	FVector2D DesignedSize(800, 220);
	FVector2D Pos = (DesignedRez * 0.5f) - (DesignedSize * 0.5f);
	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Canvas,SCanvas)

			// We use a Canvas Slot to position and size the dialog.  
			+SCanvas::Slot()
			.Position(Pos)
			.Size(DesignedSize)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Left)
			[
				// This is our primary overlay.  It controls all of the various elements of the dialog.  This is not
				// the content overlay.  This comes below.
				SNew(SOverlay)				

				// this is the background image
				+SOverlay::Slot()							
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UT.Dialog.Background"))
					]
				]

				// This will define a vertical box that holds the various components of the dialog box.
				+ SOverlay::Slot()							
				[
					SNew(SVerticalBox)

					// The title bar
					+ SVerticalBox::Slot()						
					.Padding(0.0f, 5.0f, 0.0f, 5.0f)
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Center)
					[
						SNew(SBox)
						.HeightOverride(46)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("QuickMatch","SearchingForGame","FINDING A GAME TO JOIN"))
							.TextStyle(SUWindowsStyle::Get(), "UT.Dialog.TitleTextStyle")
						]
					]
					+ SVerticalBox::Slot()
					.Padding(12.0f, 25.0f, 12.0f, 25.0f)
					.AutoHeight()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &SUTQuickMatch::GetStatusText)
							.TextStyle(SUWindowsStyle::Get(), "UT.Dialog.BodyTextStyle")
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &SUTQuickMatch::GetMinorStatusText)
							.TextStyle(SUWindowsStyle::Get(), "UT.Dialog.BodyTextTiny")
						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.AutoHeight()
						.Padding(0.0f,10.0f,0.0f,0.0f)
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
							.OnClicked(this, &SUTQuickMatch::OnCancelClick)
							.ContentPadding(FMargin(25.0, 0.0, 25.0, 5.0))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("QuickMatchg", "CancelText", "ESC to Cancel"))
									.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
								]
							]
						]
					]

				]
			]
		];


	BeginQuickmatch();
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}


FText SUTQuickMatch::GetStatusText() const
{
	int32 DeltaSeconds = int32(PlayerOwner->GetWorld()->GetTimeSeconds() - StartTime);

	if (bWaitingForMatch)
	{
		return FText::Format(NSLOCTEXT("QuickMatch","StatusFormatStrWait","Starting up Match... ({0})"), FText::AsNumber(DeltaSeconds));
	}

	return FText::Format(NSLOCTEXT("QuickMatch","StatusFormatStr","Searching for a game... ({0})"), FText::AsNumber(DeltaSeconds));
}

FText SUTQuickMatch::GetMinorStatusText() const
{
	return MinorStatusText;
}


void SUTQuickMatch::BeginQuickmatch()
{
	bCancelQuickmatch = false;
	
	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem) OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

	if (OnlineSessionInterface.IsValid())
	{
		// First step, cancel out any existing MCP searches...

		FOnCancelFindSessionsCompleteDelegate Delegate;
		Delegate.BindSP(this, &SUTQuickMatch::OnInitialFindCancel);
		OnCancelFindSessionCompleteHandle = OnlineSessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(Delegate);
		OnlineSessionInterface->CancelFindSessions();
	}
}

void SUTQuickMatch::OnInitialFindCancel(bool bWasSuccessful)
{
	// We don't really care if this succeeded since a failure just means there were
	// no sessions.

	if (OnlineSessionInterface.IsValid())
	{
		OnlineSessionInterface->ClearOnCancelFindSessionsCompleteDelegate_Handle(OnCancelFindSessionCompleteHandle);
		OnCancelFindSessionCompleteHandle.Reset();
	}

	if (bCancelQuickmatch)
	{
		PlayerOwner->CloseQuickMatch();
	}
	else
	{
		FindHUBToJoin();
	}
}

void SUTQuickMatch::FindHUBToJoin()
{
	if (OnlineSessionInterface.IsValid())
	{
		// Setup our Find complete Delegate
		FOnFindSessionsCompleteDelegate Delegate;
		Delegate.BindSP(this, &SUTQuickMatch::OnFindSessionsComplete);
		OnFindSessionCompleteHandle = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(Delegate);

		// Now look for official hubs

		SearchSettings = MakeShareable(new FUTOnlineGameSearchBase(false));
		SearchSettings->MaxSearchResults = 10000;
		FString GameVer = FString::Printf(TEXT("%i"), FNetworkVersion::GetLocalNetworkVersion());
		SearchSettings->QuerySettings.Set(SETTING_SERVERVERSION, GameVer, EOnlineComparisonOp::Equals);			// Must equal the game version
		SearchSettings->QuerySettings.Set(SETTING_GAMEINSTANCE, 1, EOnlineComparisonOp::NotEquals);				// Must not be a lobby server instance

		FString GameMode = TEXT("/Script/UnrealTournament.UTLobbyGameMode");
		SearchSettings->QuerySettings.Set(SETTING_GAMEMODE, GameMode, EOnlineComparisonOp::Equals);				// Must be a lobby server
		
		// TODO: Add the search setting for TrustLevel

		TSharedRef<FUTOnlineGameSearchBase> SearchSettingsRef = SearchSettings.ToSharedRef();

		OnlineSessionInterface->FindSessions(0, SearchSettingsRef);
		bSearchInProgress = true;

		MinorStatusText = NSLOCTEXT("QuickMatch","Status_GettingServerList","Retreiving Server List from MCP...");
	}
}


void SUTQuickMatch::OnFindSessionsComplete(bool bWasSuccessful)
{
	// Clear this delegate
	if (OnlineSessionInterface.IsValid())
	{
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteHandle);
		OnFindSessionCompleteHandle.Reset();
	}

	if (bWasSuccessful)
	{
		// Iterate through all of the hubs and first the best one...

		if (SearchSettings->SearchResults.Num() > 0)
		{
			for (int32 ServerIndex = 0; ServerIndex < SearchSettings->SearchResults.Num(); ServerIndex++)
			{
				int32 ServerFlags = 0x0000;
				SearchSettings->SearchResults[ServerIndex].Session.SessionSettings.Get(SETTING_SERVERFLAGS, ServerFlags);

				// Make sure the server we are connecting to isn't password protected.
				if ((ServerFlags & 0x01) != 0x01)
				{
					int32 NoPlayers;
					SearchSettings->SearchResults[ServerIndex].Session.SessionSettings.Get(SETTING_PLAYERSONLINE, NoPlayers);
					TSharedRef<FServerSearchInfo> NewServer = FServerSearchInfo::Make(SearchSettings->SearchResults[ServerIndex], 0, NoPlayers);
					ServerList.Add(NewServer);
				}
			}

			if (ServerList.Num() > 0)
			{
				PingNextBatch();
				return;
			}
		}
	}

	// We get here, we just force the find best match call.  This will fail and error out but insures and clean up happens
	FindBestMatch();
}

void SUTQuickMatch::NoAvailableMatches()
{
	PlayerOwner->CloseQuickMatch();
	PlayerOwner->MessageBox(NSLOCTEXT("QuickMatch", "NoServersTitle", "ONLINE FAILURE"), NSLOCTEXT("QuickMatch", "NoServerTitle", "The Online System is down for maintenance, please try again in a few minutes."));
}

void SUTQuickMatch::PingNextBatch()
{
	while (ServerList.Num() > 0 && PingTrackers.Num() < 10)
	{
		int32 Cnt = ServerList.Num() + PingTrackers.Num();
		MinorStatusText = FText::Format( NSLOCTEXT("QuickMatch","Status_PingingFormat","Pinging Servers ({0})"), FText::AsNumber(Cnt) );
		PingServer(ServerList[0]);
		ServerList.RemoveAtSwap(0);
	}

	if (ServerList.Num() == 0 && PingTrackers.Num() == 0)
	{
		FindBestMatch();
	}

}

void SUTQuickMatch::PingServer(TSharedPtr<FServerSearchInfo> ServerToPing)
{
	// Build the beacon
	AUTServerBeaconClient* Beacon = PlayerOwner->GetWorld()->SpawnActor<AUTServerBeaconClient>(AUTServerBeaconClient::StaticClass());
	if (Beacon)
	{
		ServerToPing->Beacon = Beacon;

		FString BeaconIP;
		OnlineSessionInterface->GetResolvedConnectString(ServerToPing->SearchResult, FName(TEXT("BeaconPort")), BeaconIP);

		Beacon->OnServerRequestResults = FServerRequestResultsDelegate::CreateSP(this, &SUTQuickMatch::OnServerBeaconResult);
		Beacon->OnServerRequestFailure = FServerRequestFailureDelegate::CreateSP(this, &SUTQuickMatch::OnServerBeaconFailure);
		FURL BeaconURL(nullptr, *BeaconIP, TRAVEL_Absolute);
		Beacon->InitClient(BeaconURL);
		PingTrackers.Add(FServerSearchPingTracker(ServerToPing, Beacon, PlayerOwner->GetWorld()->GetRealTimeSeconds()));
	}
}

void SUTQuickMatch::OnServerBeaconFailure(AUTServerBeaconClient* Sender)
{
	for (int32 i = 0; i < PingTrackers.Num(); i++)
	{
		if (PingTrackers[i].Beacon == Sender)
		{
			// Server is not responding, so ignore it...
			PingTrackers[i].Beacon->DestroyBeacon();
			PingTrackers.RemoveAt(i, 1);
			break;
		}
	}
	PingNextBatch();
}

void SUTQuickMatch::OnServerBeaconResult(AUTServerBeaconClient* Sender, FServerBeaconInfo ServerInfo)
{
	bool bIsBeginner = GetPlayerOwner()->IsConsideredABeginnner();
	for (int32 i = 0; i < PingTrackers.Num(); i++)
	{
		if (PingTrackers[i].Beacon == Sender)
		{
			// Discard training ground hubs if the player isn't a beginner and discard non-training ground hubs if they are one
			if ( (!bIsBeginner && PingTrackers[i].Server->bServerIsTrainingGround) ||
				 (bIsBeginner && !PingTrackers[i].Server->bServerIsTrainingGround) )
			{
				PingTrackers.RemoveAt(i, 1);
				break;
			}
			else
			{
				PingTrackers[i].Server->Ping = Sender->Ping;

				// Insert sort it in to the final list of servers by ping.
			
				bool bInserted = false;
				for (int32 Idx=0; Idx < FinalList.Num(); Idx++)
				{
					if ( (!FinalList[Idx]->bServerIsTrainingGround && bIsBeginner && PingTrackers[i].Server->bServerIsTrainingGround) ||			
						 (FinalList[Idx]->ServerTrustLevel < PingTrackers[i].Server->ServerTrustLevel) ||											
						 (FinalList[Idx]->Ping > Sender->Ping) )
					{
						// Insert here..

						FinalList.Insert(PingTrackers[i].Server, Idx);
						bInserted = true;
						break;					}
				}

				if (!bInserted) FinalList.Add(PingTrackers[i].Server);
				PingTrackers.RemoveAt(i, 1);
				break;
			}
		}
	}

	PingNextBatch();
}

void SUTQuickMatch::FindBestMatch()
{
	if (FinalList.Num() > 0)
	{
		// We know the first server has the best ping and is the oldest server (most likely to have players).  So now search forward to find a server within
		// 40ms of this ping that has the most players.

		TSharedPtr<FServerSearchInfo> BestPlayers = FinalList[0];
		TSharedPtr<FServerSearchInfo> BestPing = BestPlayers;

		for (int32 i=1;i<FinalList.Num();i++)
		{
			if (FinalList[i]->Ping < BestPing->Ping)
			{
				BestPing = FinalList[i];
			}

			if (FinalList[i]->NoPlayers > BestPlayers->NoPlayers)
			{
				BestPlayers = FinalList[i];
			}
		}

		BestServer = (BestPing->NoPlayers == 0 || BestPlayers->Ping < BestPing->Ping + PING_ALLOWANCE) ? BestPlayers : BestPing;
		AttemptQuickMatch();

	}
	else
	{
		NoAvailableMatches();
	}
}

void SUTQuickMatch::Cancel()
{
	if (bCancelQuickmatch) return;	// Quick out if we are already cancelling

	bCancelQuickmatch = true;

	// Shut down any ping trackers and clean up the lists.

	for (int32 i=0;i<PingTrackers.Num();i++)
	{
		if (PingTrackers[i].Beacon.IsValid())
		{
			PingTrackers[i].Beacon->DestroyBeacon();
		}
	}
	PingTrackers.Empty();
	FinalList.Empty();

	if (BestServer.IsValid() && BestServer->Beacon.IsValid())
	{
		BestServer->Beacon->DestroyBeacon();
	}

	if (OnlineSessionInterface.IsValid())
	{
		// Look to see if we are currently in the search phase.  If we are, we have to cancel it first.
		if (OnFindSessionCompleteHandle.IsValid())
		{
			// We clear the find session delegate to insure it's not fired while we are trying to cancel
			OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteHandle);
			OnFindSessionCompleteHandle.Reset();
		}

		// Tell the MCP to cancel if we are not already in that process.
		if (!OnCancelFindSessionCompleteHandle.IsValid())
		{
			// Start the cancel process
			FOnCancelFindSessionsCompleteDelegate Delegate;
			Delegate.BindSP(this, &SUTQuickMatch::OnSearchCancelled);
			OnCancelFindSessionCompleteHandle = OnlineSessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(Delegate);
			OnlineSessionInterface->CancelFindSessions();
		}
	}

}

void SUTQuickMatch::OnSearchCancelled(bool bWasSuccessful)
{
	if (OnlineSessionInterface.IsValid())
	{
		OnlineSessionInterface->ClearOnCancelFindSessionsCompleteDelegate_Handle(OnCancelFindSessionCompleteHandle);
		OnCancelFindSessionCompleteHandle.Reset();
	}

	PlayerOwner->CloseQuickMatch();
}

bool SUTQuickMatch::SupportsKeyboardFocus() const
{
	return true;
}


FReply SUTQuickMatch::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCancelClick();
	}

	return FReply::Unhandled();
}


FReply SUTQuickMatch::OnCancelClick()
{
	Cancel();
	return FReply::Handled();
}

void SUTQuickMatch::TellSlateIWantKeyboardFocus()

{
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}

void SUTQuickMatch::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	TArray<TWeakObjectPtr<AUTServerBeaconClient>> KillList;
	if (PingTrackers.Num() > 0)
	{
		float CurrentTime = PlayerOwner->GetWorld()->GetRealTimeSeconds();
		for (int32 i = 0; i < PingTrackers.Num(); i++)
		{
			if (CurrentTime - PingTrackers[i].PingStartTime > 5)
			{
				// This tracker is far outside what we want to consider.  Kill it...
				KillList.Add(PingTrackers[i].Beacon);
			}
		
		}

		if (KillList.Num() > 0)
		{
			for (int32 i=0; i<KillList.Num(); i++)
			{
				OnServerBeaconFailure(KillList[i].Get());
			}
		}
	}
}

void SUTQuickMatch::AttemptQuickMatch()
{
	BestServer->Beacon->OnRequestQuickplay = FServerRequestQuickplayDelegate::CreateSP(this, & SUTQuickMatch::RequestQuickPlayResults);
	BestServer->Beacon->ServerRequestQuickplay(QuickMatchType, GetPlayerOwner()->GetBaseELORank());
}


void SUTQuickMatch::RequestQuickPlayResults(AUTServerBeaconClient* Beacon, const FName& CommandCode, const FString& InstanceGuid)
{
	if ( CommandCode == EQuickMatchResults::CantJoin )
	{
		UE_LOG(UT,Log,TEXT("QuickPlay Request to server Failed.  Attempting the next one"));
		FinalList.Remove(BestServer);
		BestServer->Beacon->DestroyBeacon();

		// Restart the serarch
		FindBestMatch();		
	}
	else if (CommandCode == EQuickMatchResults::WaitingForStart || CommandCode == EQuickMatchResults::WaitingForStartNew )
	{
		UE_LOG(UT,Log,TEXT("Quickplay hub is spooling up instance"));
		bWaitingForMatch = true;
		if ( CommandCode == EQuickMatchResults::WaitingForStartNew )
		{
			PlayerOwner->QuickMatchLimitTime = PlayerOwner->GetWorld()->GetTimeSeconds() + 60.0;
		}
	}
	else if (CommandCode == EQuickMatchResults::Join)
	{
		UE_LOG(UT,Log,TEXT("Quickplay connecting to hub"));
		AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(GetPlayerOwner()->PlayerController);
		if (PC)
		{
			PC->ConnectToServerViaGUID(InstanceGuid,-1, false, false);
			PlayerOwner->CloseQuickMatch();
		}
		else
		{
			UE_LOG(UT, Warning,TEXT("Quickmatch could not cast to BasePlayerController"));
		}
	}

}

void SUTQuickMatch::OnDialogClosed()
{
	for (int32 i=0; i < FinalList.Num(); i++)
	{
		FinalList[i]->Beacon->DestroyBeacon();	
	}

}


#endif