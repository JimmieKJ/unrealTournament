// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsMainMenu.h"
#include "SUWindowsStyle.h"
#include "SUWDialog.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWControlSettingsDialog.h"
#include "SUWInputBox.h"
#include "SUWMessageBox.h"
#include "SUWGameSetupDialog.h"
#include "SUWScaleBox.h"
#include "UTGameEngine.h"
#include "Panels/SUWServerBrowser.h"
#include "Panels/SUWReplayBrowser.h"
#include "Panels/SUWStatsViewer.h"
#include "Panels/SUWCreditsPanel.h"
#include "Panels/SUTChallengePanel.h"
#include "Panels/SUHomePanel.h"
#include "Panels/SUTFragCenterPanel.h"
#include "UTEpicDefaultRulesets.h"
#include "UTReplicatedGameRuleset.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

#if !UE_SERVER

#include "UserWidget.h"

void SUWindowsMainMenu::CreateDesktop()
{
	bNeedToShowGamePanel = false;
	SUTMenuBase::CreateDesktop();
}

SUWindowsMainMenu::~SUWindowsMainMenu()
{
	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}
}

FReply SUWindowsMainMenu::OnFragCenterClick()
{
	ShowFragCenter();
	return FReply::Handled();
}

void SUWindowsMainMenu::DeactivatePanel(TSharedPtr<class SUWPanel> PanelToDeactivate)
{
	if (FragCenterPanel.IsValid()) FragCenterPanel.Reset();

	SUTMenuBase::DeactivatePanel(PanelToDeactivate);
}

void SUWindowsMainMenu::ShowFragCenter()
{
	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}


	if (!FragCenterPanel.IsValid())
	{
		SAssignNew(FragCenterPanel, SUTFragCenterPanel, PlayerOwner)
			.ViewportSize(FVector2D(1920, 1020))
			.AllowScaling(true)
			.ShowControls(false);

		if (FragCenterPanel.IsValid())
		{
			FragCenterPanel->Browse(TEXT("http://www.unrealtournament.com/fragcenter"));
			ActivatePanel(FragCenterPanel);
		}
	}
}

void SUWindowsMainMenu::SetInitialPanel()
{
	SAssignNew(HomePanel, SUHomePanel, PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}

FReply SUWindowsMainMenu::OnShowHomePanel()
{
	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}
	return SUTMenuBase::OnShowHomePanel();
}

/****************************** [ Build Sub Menus ] *****************************************/

void SUWindowsMainMenu::BuildLeftMenuBar()
{
	if (LeftMenuBar.IsValid())
	{
		LeftMenuBar->AddSlot()
		.Padding(50.0f, 0.0f, 0.0f, 0.0f)
		.AutoWidth()
		[
			AddPlayNow()
		];

		LeftMenuBar->AddSlot()
		.Padding(40.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			BuildTutorialSubMenu()
		];
		
		LeftMenuBar->AddSlot()
		.Padding(40.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			BuildWatchSubMenu()
		];
	}
}

TSharedRef<SWidget> SUWindowsMainMenu::BuildWatchSubMenu()
{
	TSharedPtr<SUTComboButton> DropDownButton = NULL;
		SNew(SBox)
	.HeightOverride(56)
	[
		SAssignNew(DropDownButton, SUTComboButton)
		.HasDownArrow(false)
		.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_REPLAYS", "WATCH"))
		.TextStyle(SUTStyle::Get(), "UT.Font.MenuBarText")
		.ContentPadding(FMargin(35.0,0.0,35.0,0.0))
		.ContentHAlign(HAlign_Left)
	];

	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Watch_FragCenter", "Frag Center"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnFragCenterClick));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Replays_YourReplays", "Your Replays"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnYourReplaysClick));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Replays_RecentReplays", "Recent Replays"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnRecentReplaysClick));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Replays_LiveGames", "Live Games"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnLiveGameReplaysClick), true);

	return DropDownButton.ToSharedRef();
}

TSharedRef<SWidget> SUWindowsMainMenu::BuildTutorialSubMenu()
{
	TSharedPtr<SUTComboButton> DropDownButton = NULL;
	SNew(SBox)
	.HeightOverride(56)
	[
		SAssignNew(DropDownButton, SUTComboButton)
		.HasDownArrow(false)
		.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_TUTORIAL", "LEARN"))
		.TextStyle(SUTStyle::Get(), "UT.Font.MenuBarText")
		.ContentPadding(FMargin(35.0,0.0,35.0,0.0))
		.ContentHAlign(HAlign_Left)
	];

	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Tutorial_LeanHoToPlay", "Basic Training"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnBootCampClick));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Tutorial_Community", "Training Videos"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnCommunityClick), true);

	return DropDownButton.ToSharedRef();

}


TSharedRef<SWidget> SUWindowsMainMenu::AddPlayNow()
{
	TSharedPtr<SUTComboButton> DropDownButton = NULL;

	SNew(SBox)
	.HeightOverride(56)
	[
		SAssignNew(DropDownButton, SUTComboButton)
		.HasDownArrow(false)
		.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch", "PLAY"))
		.TextStyle(SUTStyle::Get(), "UT.Font.MenuBarText")
		.ContentPadding(FMargin(35.0,0.0,35.0,0.0))
		.ContentHAlign(HAlign_Left)
	];

	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_PlayDM", "QuickPlay Deathmatch"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnPlayQuickMatch,	EEpicDefaultRuleTags::Deathmatch));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_PlayCTF", "QuickPlay Capture the Flag"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnPlayQuickMatch, EEpicDefaultRuleTags::CTF));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_CreateGame", "Create a Game"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnShowGamePanel));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_FindGame", "Find a Game..."), FOnClicked::CreateSP(this, &SUTMenuBase::OnShowServerBrowserPanel));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_IPConnect", "Connect via IP"), FOnClicked::CreateSP(this, &SUWindowsMainMenu::OnConnectIP), true);

	return DropDownButton.ToSharedRef();
}

FReply SUWindowsMainMenu::OnCloseClicked()
{

	for (int32 i=0; i<AvailableGameRulesets.Num();i++)
	{
		AvailableGameRulesets[i]->SlateBadge = NULL;
	}


	PlayerOwner->HideMenu();
	ConsoleCommand(TEXT("quit"));
	return FReply::Handled();
}



FReply SUWindowsMainMenu::OnShowGamePanel()
{
	ShowGamePanel();
	return FReply::Handled();
}

void SUWindowsMainMenu::ShowGamePanel()
{
	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}

	if ( !ChallengePanel.IsValid() )
	{
		SAssignNew(ChallengePanel, SUTChallengePanel, PlayerOwner);
	}

	ActivatePanel(ChallengePanel);
}

void SUWindowsMainMenu::ShowCustomGamePanel()
{
	if (TickCountDown <= 0)
	{
		PlayerOwner->ShowContentLoadingMessage();
		bNeedToShowGamePanel = true;
		TickCountDown = 3;
	}
}

void SUWindowsMainMenu::OpenDelayedMenu()
{
	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}

	SUTMenuBase::OpenDelayedMenu();
	if (bNeedToShowGamePanel)
	{
		bNeedToShowGamePanel = false;
		AvailableGameRulesets.Empty();
		TArray<FString> AllowedGameRulesets;

		UUTEpicDefaultRulesets* DefaultRulesets = UUTEpicDefaultRulesets::StaticClass()->GetDefaultObject<UUTEpicDefaultRulesets>();
		if (DefaultRulesets && DefaultRulesets->AllowedRulesets.Num() > 0)
		{
			AllowedGameRulesets.Append(DefaultRulesets->AllowedRulesets);
		}

		// If someone has screwed up the ini completely, just load all of the Epic defaults
		if (AllowedGameRulesets.Num() <= 0)
		{
			UUTEpicDefaultRulesets::GetEpicRulesets(AllowedGameRulesets);
		}

		// Grab all of the available map assets.
		TArray<FAssetData> MapAssets;
		GetAllAssetData(UWorld::StaticClass(), MapAssets);

		UE_LOG(UT,Verbose,TEXT("Loading Settings for %i Rules"), AllowedGameRulesets.Num())
		for (int32 i=0; i < AllowedGameRulesets.Num(); i++)
		{
			UE_LOG(UT,Verbose,TEXT("Loading Rule %s"), *AllowedGameRulesets[i])
			if (!AllowedGameRulesets[i].IsEmpty())
			{
				FName RuleName = FName(*AllowedGameRulesets[i]);
				UUTGameRuleset* NewRuleset = NewObject<UUTGameRuleset>(GetTransientPackage(), RuleName, RF_Transient);
				if (NewRuleset)
				{
					NewRuleset->UniqueTag = AllowedGameRulesets[i];
					bool bExistsAlready = false;
					for (int32 j=0; j < AvailableGameRulesets.Num(); j++)
					{
						if ( AvailableGameRulesets[j]->UniqueTag.Equals(NewRuleset->UniqueTag, ESearchCase::IgnoreCase) || AvailableGameRulesets[j]->Title.ToLower() == NewRuleset->Title.ToLower() )
						{
							bExistsAlready = true;
							break;
						}
					}

					if ( !bExistsAlready )
					{
						// Before we create the replicated version of this rule.. if it's an epic rule.. insure they are using our defaults.
						UUTEpicDefaultRulesets::InsureEpicDefaults(NewRuleset);

						FActorSpawnParameters Params;
						Params.Owner = PlayerOwner->GetWorld()->GetGameState();
						AUTReplicatedGameRuleset* NewReplicatedRuleset = PlayerOwner->GetWorld()->SpawnActor<AUTReplicatedGameRuleset>(Params);
						if (NewReplicatedRuleset)
						{
							// Build out the map info
							NewReplicatedRuleset->SetRules(NewRuleset, MapAssets);
							AvailableGameRulesets.Add(NewReplicatedRuleset);
						}
					}
					else
					{
						UE_LOG(UT,Verbose,TEXT("Rule %s already exists."), *AllowedGameRulesets[i]);
					}
				}
			}
		}
	
		for (int32 i=0; i < AvailableGameRulesets.Num(); i++)
		{
			AvailableGameRulesets[i]->BuildSlateBadge();
		}

		if (AvailableGameRulesets.Num() > 0)
		{
			
			SAssignNew(CreateGameDialog, SUWGameSetupDialog)
			.PlayerOwner(PlayerOwner)
			.GameRuleSets(AvailableGameRulesets)
			.ButtonMask(UTDIALOG_BUTTON_PLAY | UTDIALOG_BUTTON_CANCEL | UTDIALOG_BUTTON_LAN)
			.OnDialogResult(this, &SUWindowsMainMenu::OnGameChangeDialogResult);
		

			if ( CreateGameDialog.IsValid() )
			{
				PlayerOwner->OpenDialog(CreateGameDialog.ToSharedRef(), 100);
			}
	
		}
	}
	PlayerOwner->HideContentLoadingMessage();
}

void SUWindowsMainMenu::OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed)
{
	if ( ButtonPressed != UTDIALOG_BUTTON_CANCEL && CreateGameDialog.IsValid() )
	{
		if (ButtonPressed == UTDIALOG_BUTTON_PLAY)
		{
			StartGame(false);
		}
		else
		{
			CheckLocalContentForLanPlay();
		}
	}
}

FReply SUWindowsMainMenu::OnPlayQuickMatch(FString QuickMatchType)
{
	QuickPlay(QuickMatchType);
	return FReply::Handled();
}


void SUWindowsMainMenu::QuickPlay(const FString& QuickMatchType)
{
	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->GetAuth();
		return;
	}

	UE_LOG(UT,Log,TEXT("QuickMatch: %s"),*QuickMatchType);
	PlayerOwner->StartQuickMatch(QuickMatchType);
}

FReply SUWindowsMainMenu::OnBootCampClick()
{
	OpenTutorialMenu();
	return FReply::Handled();
}

void SUWindowsMainMenu::OpenTutorialMenu()
{
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine)
	{
		if ((!TutorialMenu.IsValid() || !TutorialMenu->IsInViewport()) && UTEngine->TutorialMenuClass != NULL)
		{
			TutorialMenu = CreateWidget<UUserWidget>(PlayerOwner->GetWorld(), UTEngine->TutorialMenuClass);
			if (TutorialMenu != NULL)
			{
				TutorialMenu->AddToViewport(0);
			}
		}
	}
}


FReply SUWindowsMainMenu::OnYourReplaysClick()
{
	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}


	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline( TEXT( "" ), TEXT( "" ) );
		return FReply::Handled();
	}

	TSharedPtr<class SUWReplayBrowser> ReplayBrowser = PlayerOwner->GetReplayBrowser();
	if (ReplayBrowser.IsValid())
	{
		ReplayBrowser->bLiveOnly = false;
		ReplayBrowser->bShowReplaysFromAllUsers = false;
		ReplayBrowser->MetaString = TEXT("");

		if (ReplayBrowser == ActivePanel)
		{
			ReplayBrowser->BuildReplayList(PlayerOwner->GetPreferredUniqueNetId()->ToString());
		}
		else
		{
			ActivatePanel(ReplayBrowser);
		}
	}

	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnRecentReplaysClick()
{
	RecentReplays();
	return FReply::Handled();
}

void SUWindowsMainMenu::RecentReplays()
{
	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}


	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline( TEXT( "" ), TEXT( "" ) );
		return;
	}

	TSharedPtr<class SUWReplayBrowser> ReplayBrowser = PlayerOwner->GetReplayBrowser();
	if (ReplayBrowser.IsValid())
	{
		ReplayBrowser->bLiveOnly = false;
		ReplayBrowser->bShowReplaysFromAllUsers = true;
		ReplayBrowser->MetaString = TEXT("");

		if (ReplayBrowser == ActivePanel)
		{
			ReplayBrowser->BuildReplayList(TEXT(""));
		}
		else
		{
			ActivatePanel(ReplayBrowser);
		}
	}

}

FReply SUWindowsMainMenu::OnLiveGameReplaysClick()
{
	ShowLiveGameReplays();

	return FReply::Handled();
}

void SUWindowsMainMenu::ShowLiveGameReplays()
{

	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}


	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline( TEXT( "" ), TEXT( "" ) );
		return;
	}

	TSharedPtr<class SUWReplayBrowser> ReplayBrowser = PlayerOwner->GetReplayBrowser();
	if (ReplayBrowser.IsValid())
	{
		ReplayBrowser->bLiveOnly = true;
		ReplayBrowser->bShowReplaysFromAllUsers = true;
		ReplayBrowser->MetaString = TEXT("");

		if (ReplayBrowser == ActivePanel)
		{
			ReplayBrowser->BuildReplayList(TEXT(""));
		}
		else
		{
			ActivatePanel(ReplayBrowser);
		}
	}

	return;
}

FReply SUWindowsMainMenu::OnCommunityClick()
{
	ShowCommunity();
	return FReply::Handled();
}

void SUWindowsMainMenu::ShowCommunity()
{

	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}


	if ( !WebPanel.IsValid() )
	{
		// Create the Web panel
		SAssignNew(WebPanel, SUTWebBrowserPanel, PlayerOwner);
	}

	if (WebPanel.IsValid())
	{
		if (ActivePanel.IsValid() && ActivePanel != WebPanel)
		{
			ActivatePanel(WebPanel);
		}
		WebPanel->Browse(CommunityVideoURL);
	}
}

FReply SUWindowsMainMenu::OnConnectIP()
{
	PlayerOwner->OpenDialog(
							SNew(SUWInputBox)
							.DefaultInput(PlayerOwner->LastConnectToIP)
							.DialogSize(FVector2D(700, 300))
							.OnDialogResult(this, &SUWindowsMainMenu::ConnectIPDialogResult)
							.PlayerOwner(PlayerOwner)
							.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "ConnectToIP", "Connect to IP"))
							.MessageText(NSLOCTEXT("SUWindowsDesktop", "ConnecToIPDesc", "Enter address to connect to:"))
							.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
							.IsScrollable(false)
							);
	return FReply::Handled();
}

void SUWindowsMainMenu::ConnectIPDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		TSharedPtr<SUWInputBox> Box = StaticCastSharedPtr<SUWInputBox>(Widget);
		if (Box.IsValid())
		{
			FString InputText = Box->GetInputText();
			if (InputText.Len() > 0 && PlayerOwner.IsValid())
			{
				FString AdjustedText = InputText.Replace(TEXT("://"), TEXT(""));
				PlayerOwner->LastConnectToIP = AdjustedText;
				PlayerOwner->SaveConfig();
				PlayerOwner->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("open %s"), *AdjustedText));
				PlayerOwner->ShowConnectingDialog();
			}
		}
	}
}

bool SUWindowsMainMenu::ShouldShowBrowserIcon()
{
	return (PlayerOwner.IsValid() && PlayerOwner->bShowBrowserIconOnMainMenu);
}

void SUWindowsMainMenu::CheckLocalContentForLanPlay()
{
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (!UTEngine->IsCloudAndLocalContentInSync())
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("SUWCreateGamePanel", "CloudSyncErrorCaption", "Cloud Not Synced"), NSLOCTEXT("SUWCreateGamePanel", "CloudSyncErrorMsg", "Some files are not up to date on your cloud storage. Would you like to start anyway?"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUWindowsMainMenu::CloudOutOfSyncResult));
	}
	else
	{
		UUTGameUserSettings* GameSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

		bool bShowingLanWarning = false;
		if( !GameSettings->bShouldSuppressLanWarning )
		{
			TWeakObjectPtr<UUTLocalPlayer> LP = PlayerOwner;
			auto OnDialogConfirmation = [LP] (TSharedPtr<SCompoundWidget> Widget, uint16 Button)
			{
				UUTGameUserSettings* LamdaGameSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
				LamdaGameSettings->SaveConfig();
			};

			bool bCanBindAll = false;
			TSharedRef<FInternetAddr> Address = ISocketSubsystem::Get()->GetLocalHostAddr(*GWarn, bCanBindAll);
			if (Address->IsValid())
			{
				FString StringAddress = Address->ToString(false);

				// Note: This is an extremely basic way to test for local network and it only covers the common case
				if (StringAddress.StartsWith(TEXT("192.168.")))
				{
					bShowingLanWarning = true;
					PlayerOwner->ShowSupressableConfirmation(
						NSLOCTEXT("SUWCreateGamePanel", "LocalNetworkWarningTitle", "Local Network Detected"),
						NSLOCTEXT("SUWCreateGamePanel", "LocalNetworkWarningDesc", "Make sure ports 7777 and 15000 are forwarded in your router to be visible to players outside your local network"),
						FVector2D(0, 0),
						GameSettings->bShouldSuppressLanWarning,
						FDialogResultDelegate::CreateLambda( OnDialogConfirmation ) );
				}

			}
		}
		
		UUTGameEngine* Engine = Cast<UUTGameEngine>(GEngine);
		if (Engine != NULL && !Engine->IsCloudAndLocalContentInSync())
		{
			FText DialogText = NSLOCTEXT("UT", "ContentOutOfSyncWarning", "You have locally created custom content that is not in your cloud storage. Players may be unable to join your server. Are you sure?");
			FDialogResultDelegate Callback;
			Callback.BindSP(this, &SUWindowsMainMenu::StartGameWarningComplete);
			PlayerOwner->ShowMessage(NSLOCTEXT("U1T", "ContentNotInSync", "Custom Content Out of Sync"), DialogText, UTDIALOG_BUTTON_YES | UTDIALOG_BUTTON_NO, Callback);
		}
		else
		{
			StartGame(true);
		}
	}
	
}

void SUWindowsMainMenu::StartGameWarningComplete(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES || ButtonID == UTDIALOG_BUTTON_OK)
	{
		StartGame(true);
	}
}


void SUWindowsMainMenu::StartGame(bool bLanGame)
{

	// Kill any existing Dedicated servers
	if (PlayerOwner->DedicatedServerProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(PlayerOwner->DedicatedServerProcessHandle,true);
		PlayerOwner->DedicatedServerProcessHandle.Reset();
	}


	if (FUTAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> ParamArray;
		if (bLanGame)		
		{
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("StartGameMode"), TEXT("Listen")));
		}
		else
		{
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("StartGameMode"), TEXT("Standalone")));
		}
		FUTAnalytics::GetProvider().RecordEvent( TEXT("MenuStartGame"), ParamArray );
	}

	FString StartingMap;
	FString GameOptions;

	if (CreateGameDialog->IsCustomSettings())
	{
		FString GameMode;
		FString Description;
		TArray<FString> GameOptionsList;

		int32 DesiredPlayerCount = 0;
		int32 bTeamGame;

		CreateGameDialog->GetCustomGameSettings(GameMode, StartingMap, Description, GameOptionsList, DesiredPlayerCount,bTeamGame);	

		GameOptions = FString::Printf(TEXT("?Game=%s"), *GameMode);
		for (int32 i = 0; i < GameOptionsList.Num(); i++)
		{
			GameOptions += FString::Printf(TEXT("?%s"),*GameOptionsList[i]);
		}

		if (CreateGameDialog->BotSkillLevel >= 0)
		{
			GameOptions += FString::Printf(TEXT("?Difficulty=%i?BotFill=%i?MaxPlayers=%i"),CreateGameDialog->BotSkillLevel, DesiredPlayerCount, DesiredPlayerCount);
		}
		else
		{
			GameOptions += FString::Printf(TEXT("?BotFill=0?MaxPlayers=%i"), DesiredPlayerCount);
		}

	}
	else
	{
		// Build the settings from the ruleset...

		AUTReplicatedGameRuleset* CurrentRule = CreateGameDialog->SelectedRuleset.Get();
	
		StartingMap = CreateGameDialog->GetSelectedMap();
		
		AUTGameMode* DefaultGameMode = CurrentRule->GetDefaultGameModeObject();

		GameOptions = FString::Printf(TEXT("?Game=%s"), *CurrentRule->GameMode);
		GameOptions += FString::Printf(TEXT("?MaxPlayers=%i"), CurrentRule->MaxPlayers);
		GameOptions += CurrentRule->GameOptions;
		if ( DefaultGameMode && CreateGameDialog->BotSkillLevel >= 0 )
		{
			// This match wants bots.  
			int32 OptimalPlayerCount = DefaultGameMode->bTeamGame ? CreateGameDialog->MapPlayList[0].MapInfo->OptimalTeamPlayerCount : CreateGameDialog->MapPlayList[0].MapInfo->OptimalPlayerCount;
			GameOptions += FString::Printf(TEXT("?BotFill=%i?Difficulty=%i"), OptimalPlayerCount, FMath::Clamp<int32>(CreateGameDialog->BotSkillLevel,0,7));				
		}
		else
		{
			GameOptions += TEXT("?BotFill=0");
		}
	}


	for (int32 i=0; i<AvailableGameRulesets.Num();i++)
	{
		AvailableGameRulesets[i]->SlateBadge = NULL;
	}

	FString URL = StartingMap + GameOptions;

	if (bLanGame)
	{
		FString ExecPath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());
		FString Options = FString::Printf(TEXT("unrealtournament %s -log -server"), *URL);

		IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
		if (OnlineSubsystem)
		{
			IOnlineIdentityPtr OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
			if (OnlineIdentityInterface.IsValid())
			{
				TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(PlayerOwner->GetControllerId());
				if (UserId.IsValid())
				{
					Options += FString::Printf(TEXT(" -cloudID=%s"), *UserId->ToString());
				}
			}
		}

		FString McpConfigOverride;
		if (FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), McpConfigOverride))
		{
			Options += FString::Printf(TEXT(" -MCPCONFIG=%s"), *McpConfigOverride);
		}

		PlayerOwner->DedicatedServerProcessHandle = FPlatformProcess::CreateProc(*ExecPath, *(Options + FString::Printf(TEXT(" -ClientProcID=%u"), FPlatformProcess::GetCurrentProcessId())), true, false, false, NULL, 0, NULL, NULL);
		if (PlayerOwner->DedicatedServerProcessHandle.IsValid())
		{
			GEngine->SetClientTravel(PlayerOwner->PlayerController->GetWorld(), TEXT("127.0.0.1"), TRAVEL_Absolute);
			PlayerOwner->HideMenu();
		}
	}
	else
	{
		ConsoleCommand(TEXT("Open ") + URL);
	}

	if (CreateGameDialog.IsValid())
	{
		CreateGameDialog.Reset();
	}
}


void SUWindowsMainMenu::CloudOutOfSyncResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		StartGame(true);
	}
}

FReply SUWindowsMainMenu::OnShowServerBrowserPanel()
{
	if (TutorialMenu.IsValid())
	{
		TutorialMenu->RemoveFromViewport();
	}

	return SUTMenuBase::OnShowServerBrowserPanel();

}

void SUWindowsMainMenu::OnMenuOpened(const FString& Parameters)
{
	SUWindowsDesktop::OnMenuOpened(Parameters);
	if (Parameters.Equals(TEXT("showchallenge"), ESearchCase::IgnoreCase))
	{
		ShowGamePanel();
	}
}

void SUWindowsMainMenu::OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	if (NewStatus == ELoginStatus::LoggedIn)
	{
		if (TutorialMenu.IsValid() && TutorialMenu->IsInViewport())
		{
			TutorialMenu->RemoveFromViewport();
			
			PlayerOwner->ShowMessage(
				NSLOCTEXT("SUWindowsMainMenu", "TutorialErrorTitle", "User Changed"),
				NSLOCTEXT("SUWindowsMainMenu", "TutorialErrorMessage", "User changed. Returning to the main menu."), UTDIALOG_BUTTON_OK, nullptr
				);
		}
	}

	SUTMenuBase::OnOwnerLoginStatusChanged(LocalPlayerOwner, NewStatus, UniqueID);
}

#endif