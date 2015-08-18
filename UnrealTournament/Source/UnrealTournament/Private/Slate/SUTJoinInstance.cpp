// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUTJoinInstance.h"
#include "SUWindowsStyle.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "OnlineSubsystemTypes.h"
#include "Panels/SUWServerBrowser.h"
#include "UTGameEngine.h"
#include "UTServerBeaconClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "UnrealNetwork.h"

#if !UE_SERVER

void SUTJoinInstance::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	ServerData = InArgs._ServerData;
	InstanceId = InArgs._InstanceId;
	bSpectator = InArgs._bSpectator;

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
							.Text(NSLOCTEXT("JoinInstance","JoiningGame","Joining Game"))
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
							.Text(this, &SUTJoinInstance::GetStatusText)
							.TextStyle(SUWindowsStyle::Get(), "UT.Dialog.BodyTextStyle")
						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.AutoHeight()
						.Padding(0.0f,10.0f,0.0f,0.0f)
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
							.OnClicked(this, &SUTJoinInstance::OnCancelClick)
							.ContentPadding(FMargin(25.0, 0.0, 25.0, 5.0))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("QuickMatch", "CancelText", "ESC to Cancel"))
									.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
								]
							]
						]
					]

				]
			]
		];


	BeginJoin();
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}


FText SUTJoinInstance::GetStatusText() const
{
	int32 DeltaSeconds = int32(PlayerOwner->GetWorld()->GetTimeSeconds() - StartTime);
	return FText::Format(NSLOCTEXT("QuickJoin","TalkingToServer","Talking to Hub... ({0})"), FText::AsNumber(DeltaSeconds));
}


void SUTJoinInstance::BeginJoin()
{
	bCancelJoin = false;
	
	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem) OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

	if (OnlineSessionInterface.IsValid())
	{
		// First step, cancel out any existing MCP searches...

		FOnCancelFindSessionsCompleteDelegate Delegate;
		Delegate.BindSP(this, &SUTJoinInstance::OnInitialFindCancel);
		OnCancelFindSessionCompleteHandle = OnlineSessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(Delegate);
		OnlineSessionInterface->CancelFindSessions();
	}

}

void SUTJoinInstance::OnInitialFindCancel(bool bWasSuccessful)
{

	// We don't really care if this succeeded since a failure just means there were
	// no sessions.

	if (OnlineSessionInterface.IsValid())
	{
		OnlineSessionInterface->ClearOnCancelFindSessionsCompleteDelegate_Handle(OnCancelFindSessionCompleteHandle);
		OnCancelFindSessionCompleteHandle.Reset();
	}

	if (bCancelJoin)
	{
		PlayerOwner->CloseJoinInstanceDialog();
	}
	else
	{
		RequestJoinFromHub();		
	}

}

void SUTJoinInstance::RequestJoinFromHub()
{
	// Build the beacon
	Beacon = PlayerOwner->GetWorld()->SpawnActor<AUTServerBeaconClient>(AUTServerBeaconClient::StaticClass());
	if (Beacon.IsValid())
	{
		FString BeaconIP;
		OnlineSessionInterface->GetResolvedConnectString(ServerData->SearchResult, FName(TEXT("BeaconPort")), BeaconIP);

		Beacon->OnServerRequestResults = FServerRequestResultsDelegate::CreateSP(this, &SUTJoinInstance::OnServerBeaconResult);
		Beacon->OnServerRequestFailure = FServerRequestFailureDelegate::CreateSP(this, &SUTJoinInstance::OnServerBeaconFailure);

		FURL BeaconURL(nullptr, *BeaconIP, TRAVEL_Absolute);
		Beacon->InitClient(BeaconURL);
	}
}

void SUTJoinInstance::OnServerBeaconFailure(AUTServerBeaconClient* Sender)
{
	// Error message
}

void SUTJoinInstance::OnServerBeaconResult(AUTServerBeaconClient* Sender, FServerBeaconInfo ServerInfo)
{
	Sender->OnRequestJoinInstanceResult = FServerRequestJoinInstanceResult::CreateSP(this, &SUTJoinInstance::OnRequestJoinResult);
	Sender->ServerRequestInstanceJoin(InstanceId, bSpectator, PlayerOwner->GetBaseELORank());
}

void SUTJoinInstance::OnRequestJoinResult(EInstanceJoinResult::Type Result, const FString& Params)
{
	// Close Ourself.
	PlayerOwner->CloseJoinInstanceDialog();

	if (Result == EInstanceJoinResult::MatchNoLongerExists)
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("InstanceTravelFailures","CouldNotTravel","Could not travel"), NSLOCTEXT("InstanceTravelFailures","MatchNoLongerExists","The match you are trying to join is no longer being played.  Please select another"),UTDIALOG_BUTTON_OK);
	}
	else if (Result == EInstanceJoinResult::MatchRankFail)
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("InstanceTravelFailures","CouldNotTravel","Could not travel"), NSLOCTEXT("InstanceTravelFailures","MatchRankLocked","The match you are trying to join has been locked to ranks lower than yours.  Please select another."),UTDIALOG_BUTTON_OK);
	}
	else if (Result == EInstanceJoinResult::MatchLocked)
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("InstanceTravelFailures","CouldNotTravel","Could not travel"), NSLOCTEXT("InstanceTravelFailures","MatchLocked","The match you are trying to join has been made private.  Please select another."),UTDIALOG_BUTTON_OK);
	}
	else if (Result == EInstanceJoinResult::JoinViaLobby)
	{
		PlayerOwner->JoinSession(ServerData->SearchResult, bSpectator, NAME_None, false, -1, Params);			
	}
	else if (Result == EInstanceJoinResult::JoinDirectly)
	{
		AUTBasePlayerController* UTPlayerController = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
		if (UTPlayerController)
		{
			UTPlayerController->ConnectToServerViaGUID(Params,-1, bSpectator,false);
		}
		else
		{
			// LIE! :)  But we should never get here!
			PlayerOwner->ShowMessage(NSLOCTEXT("InstanceTravelFailures","CouldNotTravel","Could not travel"), NSLOCTEXT("InstanceTravelFailures","MatchNoLongerExists","The match you are trying to join is no longer being played.  Please select another"),UTDIALOG_BUTTON_OK);
		}
	}
}

void SUTJoinInstance::Cancel()
{
	if (bCancelJoin) return;	// Quick out if we are already cancelling

	bCancelJoin= true;

	Beacon->DestroyBeacon();

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
			Delegate.BindSP(this, &SUTJoinInstance::OnSearchCancelled);
			OnCancelFindSessionCompleteHandle = OnlineSessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(Delegate);
			OnlineSessionInterface->CancelFindSessions();
		}
	}

}

void SUTJoinInstance::OnSearchCancelled(bool bWasSuccessful)
{
	if (OnlineSessionInterface.IsValid())
	{
		OnlineSessionInterface->ClearOnCancelFindSessionsCompleteDelegate_Handle(OnCancelFindSessionCompleteHandle);
		OnCancelFindSessionCompleteHandle.Reset();
	}

	PlayerOwner->CloseJoinInstanceDialog();
}

bool SUTJoinInstance::SupportsKeyboardFocus() const
{
	return true;
}


FReply SUTJoinInstance::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCancelClick();
	}

	return FReply::Unhandled();
}


FReply SUTJoinInstance::OnCancelClick()
{
	Cancel();
	return FReply::Handled();
}

void SUTJoinInstance::TellSlateIWantKeyboardFocus()
{
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}


void SUTJoinInstance::OnDialogClosed()
{
	if (Beacon.IsValid())
	{
		Beacon->DestroyBeacon();	
	}
}


#endif