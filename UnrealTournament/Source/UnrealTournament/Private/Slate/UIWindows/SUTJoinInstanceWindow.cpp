// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTJoinInstanceWindow.h"
#include "../SUTStyle.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "OnlineSubsystemTypes.h"
#include "../Panels/SUTServerBrowserPanel.h"
#include "UTGameEngine.h"
#include "UTServerBeaconClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "UnrealNetwork.h"

#if !UE_SERVER

void SUTJoinInstanceWindow::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	PlayerOwner = InPlayerOwner;
	ServerData = InArgs._ServerData;
	InstanceId = InArgs._InstanceId;
	bSpectator = InArgs._bSpectator;
	StartTime = PlayerOwner->GetWorld()->GetTimeSeconds();
	bWaitingForMatch = false;

	SUTWindowBase::Construct
	(
		SUTWindowBase::FArguments()
			.Size(FVector2D(800,220))
			.bSizeIsRelative(false)
			.Position(FVector2D(0.5f, 0.5f))
			.AnchorPoint(FVector2D(0.5f,0.5f))
			.bShadow(true)

		,PlayerOwner
			
	);
}

void SUTJoinInstanceWindow::BuildWindow()
{
	// this is the background image
	Content->AddSlot()							
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
		]
	];

	// This will define a vertical box that holds the various components of the dialog box.
	Content->AddSlot()
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
				.Text(this, &SUTJoinInstanceWindow::GetStatusText)
				.TextStyle(SUWindowsStyle::Get(), "UT.Dialog.BodyTextStyle")
			]
			+SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoHeight()
			.Padding(0.0f,10.0f,0.0f,0.0f)
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
				.OnClicked(this, &SUTJoinInstanceWindow::OnCancelClick)
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

	];

	BeginJoin();
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}


FText SUTJoinInstanceWindow::GetStatusText() const
{
	int32 DeltaSeconds = int32(PlayerOwner->GetWorld()->GetTimeSeconds() - StartTime);
	return FText::Format(NSLOCTEXT("QuickJoin","TalkingToServer","Talking to Hub... ({0})"), FText::AsNumber(DeltaSeconds));
}


void SUTJoinInstanceWindow::BeginJoin()
{
	bCancelJoin = false;
	
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	IOnlineSessionPtr OnlineSessionInterface;
	if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

	if (OnlineSessionInterface.IsValid())
	{
		// First step, cancel out any existing MCP searches...

		FOnCancelFindSessionsCompleteDelegate Delegate;
		Delegate.BindSP(this, &SUTJoinInstanceWindow::OnInitialFindCancel);
		OnCancelFindSessionCompleteHandle = OnlineSessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(Delegate);
		OnlineSessionInterface->CancelFindSessions();
	}

}

void SUTJoinInstanceWindow::OnInitialFindCancel(bool bWasSuccessful)
{

	// We don't really care if this succeeded since a failure just means there were
	// no sessions.
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	IOnlineSessionPtr OnlineSessionInterface;
	if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

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

void SUTJoinInstanceWindow::RequestJoinFromHub()
{
	// Build the beacon
	Beacon = PlayerOwner->GetWorld()->SpawnActor<AUTServerBeaconClient>(AUTServerBeaconClient::StaticClass());
	if (Beacon.IsValid())
	{
		FString BeaconIP;
		IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
		IOnlineSessionPtr OnlineSessionInterface;
		if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
		if (OnlineSessionInterface.IsValid())
		{
			OnlineSessionInterface->GetResolvedConnectString(ServerData->SearchResult, FName(TEXT("BeaconPort")), BeaconIP);
		}

		Beacon->OnServerRequestResults = FServerRequestResultsDelegate::CreateSP(this, &SUTJoinInstanceWindow::OnServerBeaconResult);
		Beacon->OnServerRequestFailure = FServerRequestFailureDelegate::CreateSP(this, &SUTJoinInstanceWindow::OnServerBeaconFailure);

		FURL BeaconURL(nullptr, *BeaconIP, TRAVEL_Absolute);
		Beacon->InitClient(BeaconURL);
	}
}

void SUTJoinInstanceWindow::OnServerBeaconFailure(AUTServerBeaconClient* Sender)
{
	// Error message
}

void SUTJoinInstanceWindow::OnServerBeaconResult(AUTServerBeaconClient* Sender, FServerBeaconInfo ServerInfo)
{
	Sender->OnRequestJoinInstanceResult = FServerRequestJoinInstanceResult::CreateSP(this, &SUTJoinInstanceWindow::OnRequestJoinResult);

	int32 RankCheck = DEFAULT_RANK_CHECK;
	AUTBaseGameMode* BaseGameMode = nullptr;
	if (ServerData.IsValid())
	{
		FString GameModeClassname = ServerData->GetInstanceGameModeClass(InstanceId);
		if (!GameModeClassname.IsEmpty())
		{
			UClass* GameModeClass = LoadClass<AUTGameMode>(NULL, *GameModeClassname, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
			if (GameModeClass)
			{
				BaseGameMode = GameModeClass->GetDefaultObject<AUTBaseGameMode>();
			}
		}
	}

	if (BaseGameMode == nullptr) BaseGameMode = AUTBaseGameMode::StaticClass()->GetDefaultObject<AUTBaseGameMode>();

	AUTPlayerState* PlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
	if (PlayerState)
	{
		RankCheck = PlayerState->GetRankCheck(BaseGameMode);
	}

	Sender->ServerRequestInstanceJoin(InstanceId, bSpectator, RankCheck);
}

void SUTJoinInstanceWindow::OnRequestJoinResult(EInstanceJoinResult::Type Result, const FString& Params)
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
		PlayerOwner->JoinSession(ServerData->SearchResult, bSpectator, -1, Params);			
	}
	else if (Result == EInstanceJoinResult::JoinDirectly)
	{
		AUTBasePlayerController* UTPlayerController = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
		if (UTPlayerController)
		{
			UTPlayerController->ConnectToServerViaGUID(Params,-1, bSpectator);
		}
		else
		{
			// LIE! :)  But we should never get here!
			PlayerOwner->ShowMessage(NSLOCTEXT("InstanceTravelFailures","CouldNotTravel","Could not travel"), NSLOCTEXT("InstanceTravelFailures","MatchNoLongerExists","The match you are trying to join is no longer being played.  Please select another"),UTDIALOG_BUTTON_OK);
		}
	}
}

void SUTJoinInstanceWindow::Cancel()
{
	if (bCancelJoin) return;	// Quick out if we are already cancelling

	bCancelJoin= true;

	Beacon->DestroyBeacon();

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	IOnlineSessionPtr OnlineSessionInterface;
	if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
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
			Delegate.BindSP(this, &SUTJoinInstanceWindow::OnSearchCancelled);
			OnCancelFindSessionCompleteHandle = OnlineSessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(Delegate);
			OnlineSessionInterface->CancelFindSessions();
		}
	}

}

void SUTJoinInstanceWindow::OnSearchCancelled(bool bWasSuccessful)
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	IOnlineSessionPtr OnlineSessionInterface;
	if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
	if (OnlineSessionInterface.IsValid())
	{
		OnlineSessionInterface->ClearOnCancelFindSessionsCompleteDelegate_Handle(OnCancelFindSessionCompleteHandle);
		OnCancelFindSessionCompleteHandle.Reset();
	}

	PlayerOwner->CloseJoinInstanceDialog();
}

bool SUTJoinInstanceWindow::SupportsKeyboardFocus() const
{
	return true;
}


FReply SUTJoinInstanceWindow::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCancelClick();
	}

	return FReply::Unhandled();
}


FReply SUTJoinInstanceWindow::OnCancelClick()
{
	Cancel();
	return FReply::Handled();
}

void SUTJoinInstanceWindow::TellSlateIWantKeyboardFocus()
{
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}


void SUTJoinInstanceWindow::OnDialogClosed()
{
	if (Beacon.IsValid())
	{
		Beacon->DestroyBeacon();	
	}
}


#endif