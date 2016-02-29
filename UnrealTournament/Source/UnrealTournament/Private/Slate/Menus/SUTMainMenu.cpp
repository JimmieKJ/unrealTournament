// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTMainMenu.h"
#include "../SUWindowsStyle.h"
#include "../Base/SUTDialogBase.h"
#include "../Dialogs/SUTSystemSettingsDialog.h"
#include "../Dialogs/SUTPlayerSettingsDialog.h"
#include "../Dialogs/SUTControlSettingsDialog.h"
#include "../Dialogs/SUTInputBoxDialog.h"
#include "../Dialogs/SUTMessageBoxDialog.h"
#include "../Dialogs/SUTGameSetupDialog.h"
#include "../Widgets/SUTScaleBox.h"
#include "UTGameEngine.h"
#include "../Panels/SUTServerBrowserPanel.h"
#include "../Panels/SUTReplayBrowserPanel.h"
#include "../Panels/SUTStatsViewerPanel.h"
#include "../Panels/SUTCreditsPanel.h"
#include "../Panels/SUTChallengePanel.h"
#include "../Panels/SUTHomePanel.h"
#include "../Panels/SUTFragCenterPanel.h"
#include "UTEpicDefaultRulesets.h"
#include "UTReplicatedGameRuleset.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "../Panels/SUTUMGPanel.h"

#if !UE_SERVER

#include "UserWidget.h"

void SUTMainMenu::CreateDesktop()
{
	bNeedToShowGamePanel = false;
	SUTMenuBase::CreateDesktop();
}

SUTMainMenu::~SUTMainMenu()
{
}

TSharedRef<SWidget> SUTMainMenu::BuildBackground()
{
	return SNew(SOverlay)
	+SOverlay::Slot()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				SNew(SUTScaleBox)
				.bMaintainAspectRatio(false)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.HomePanel.Background"))
				]
			]
		]
	];
}

FReply SUTMainMenu::OnFragCenterClick()
{
	ShowFragCenter();
	return FReply::Handled();
}

void SUTMainMenu::DeactivatePanel(TSharedPtr<class SUTPanelBase> PanelToDeactivate)
{
	if (FragCenterPanel.IsValid()) FragCenterPanel.Reset();

	SUTMenuBase::DeactivatePanel(PanelToDeactivate);
}

void SUTMainMenu::ShowFragCenter()
{
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

void SUTMainMenu::SetInitialPanel()
{
	SAssignNew(HomePanel, SUTHomePanel, PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}

FReply SUTMainMenu::OnShowHomePanel()
{
	return SUTMenuBase::OnShowHomePanel();
}

/****************************** [ Build Sub Menus ] *****************************************/

void SUTMainMenu::BuildLeftMenuBar()
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

TSharedRef<SWidget> SUTMainMenu::BuildWatchSubMenu()
{
	TSharedPtr<SUTComboButton> DropDownButton = NULL;
		SNew(SBox)
	.HeightOverride(56)
	[
		SAssignNew(DropDownButton, SUTComboButton)
		.HasDownArrow(false)
		.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_REPLAYS", "WATCH"))
		.TextStyle(SUTStyle::Get(), "UT.Font.MenuBarText")
		.ContentPadding(FMargin(35.0,0.0,35.0,0.0))
		.ContentHAlign(HAlign_Left)
	];

	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Watch_FragCenter", "Frag Center"), FOnClicked::CreateSP(this, &SUTMainMenu::OnFragCenterClick));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Replays_YourReplays", "Your Replays"), FOnClicked::CreateSP(this, &SUTMainMenu::OnYourReplaysClick));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Replays_RecentReplays", "Recent Replays"), FOnClicked::CreateSP(this, &SUTMainMenu::OnRecentReplaysClick));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Replays_LiveGames", "Live Games"), FOnClicked::CreateSP(this, &SUTMainMenu::OnLiveGameReplaysClick), true);

	return DropDownButton.ToSharedRef();
}

TSharedRef<SWidget> SUTMainMenu::BuildTutorialSubMenu()
{
	TSharedPtr<SUTComboButton> DropDownButton = NULL;
	SNew(SBox)
	.HeightOverride(56)
	[
		SAssignNew(DropDownButton, SUTComboButton)
		.HasDownArrow(false)
		.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_TUTORIAL", "LEARN"))
		.TextStyle(SUTStyle::Get(), "UT.Font.MenuBarText")
		.ContentPadding(FMargin(35.0,0.0,35.0,0.0))
		.ContentHAlign(HAlign_Left)
	];

	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Tutorial_LeanHoToPlay", "Basic Training"), FOnClicked::CreateSP(this, &SUTMainMenu::OnBootCampClick));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Tutorial_Community", "Training Videos"), FOnClicked::CreateSP(this, &SUTMainMenu::OnCommunityClick), true);

	return DropDownButton.ToSharedRef();

}


TSharedRef<SWidget> SUTMainMenu::AddPlayNow()
{
	TSharedPtr<SUTComboButton> DropDownButton = NULL;

	SNew(SBox)
	.HeightOverride(56)
	[
		SAssignNew(DropDownButton, SUTComboButton)
		.HasDownArrow(false)
		.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_QuickMatch", "PLAY"))
		.TextStyle(SUTStyle::Get(), "UT.Font.MenuBarText")
		.ContentPadding(FMargin(35.0,0.0,35.0,0.0))
		.ContentHAlign(HAlign_Left)
	];

	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_QuickMatch_PlayDM", "QuickPlay Deathmatch"), FOnClicked::CreateSP(this, &SUTMainMenu::OnPlayQuickMatch,	EEpicDefaultRuleTags::Deathmatch));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_QuickMatch_PlayCTF", "QuickPlay Capture the Flag"), FOnClicked::CreateSP(this, &SUTMainMenu::OnPlayQuickMatch, EEpicDefaultRuleTags::CTF));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_QuickMatch_PlayTSD", "QuickPlay Showdown"), FOnClicked::CreateSP(this, &SUTMainMenu::OnPlayQuickMatch, EEpicDefaultRuleTags::TEAMSHOWDOWN));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_ChallengesGame", "Challenges"), FOnClicked::CreateSP(this, &SUTMainMenu::OnShowGamePanel));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_CreateGame", "Create a Match"), FOnClicked::CreateSP(this, &SUTMainMenu::OnShowCustomGamePanel));

	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_QuickMatch_FindGame", "Find a Match..."), FOnClicked::CreateSP(this, &SUTMenuBase::OnShowServerBrowserPanel),true);

	if (FParse::Param(FCommandLine::Get(), TEXT("PARTY")))
	{
		DropDownButton->AddSpacer();
		DropDownButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Ranked_Showdown", "Ranked 3v3 Showdown Queue"), FOnClicked::CreateSP(this, &SUTMenuBase::OnRankedShowdown), true);
	}

	return DropDownButton.ToSharedRef();
}

FReply SUTMainMenu::OnCloseClicked()
{

	for (int32 i=0; i<AvailableGameRulesets.Num();i++)
	{
		AvailableGameRulesets[i]->SlateBadge = NULL;
	}


	PlayerOwner->HideMenu();
	ConsoleCommand(TEXT("quit"));
	return FReply::Handled();
}



FReply SUTMainMenu::OnShowGamePanel()
{
	ShowGamePanel();
	return FReply::Handled();
}

FReply SUTMainMenu::OnShowCustomGamePanel()
{
	ShowCustomGamePanel();
	return FReply::Handled();
}


void SUTMainMenu::ShowGamePanel()
{
	if ( !ChallengePanel.IsValid() )
	{
		SAssignNew(ChallengePanel, SUTChallengePanel, PlayerOwner);
	}

	ActivatePanel(ChallengePanel);
}

void SUTMainMenu::ShowCustomGamePanel()
{
	if (TickCountDown <= 0)
	{
		PlayerOwner->ShowContentLoadingMessage();
		bNeedToShowGamePanel = true;
		TickCountDown = 3;
	}
}

void SUTMainMenu::OpenDelayedMenu()
{
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
			
			SAssignNew(CreateGameDialog, SUTGameSetupDialog)
			.PlayerOwner(PlayerOwner)
			.GameRuleSets(AvailableGameRulesets)
			.DialogSize(FVector2D(1920,1080))
			.ButtonMask(UTDIALOG_BUTTON_PLAY | UTDIALOG_BUTTON_CANCEL)
			.OnDialogResult(this, &SUTMainMenu::OnGameChangeDialogResult);
		

			if ( CreateGameDialog.IsValid() )
			{
				PlayerOwner->OpenDialog(CreateGameDialog.ToSharedRef(), 100);
			}
	
		}
	}
	PlayerOwner->HideContentLoadingMessage();
}

void SUTMainMenu::OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed)
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

FReply SUTMainMenu::OnPlayQuickMatch(FString QuickMatchType)
{
	QuickPlay(QuickMatchType);
	return FReply::Handled();
}


void SUTMainMenu::QuickPlay(const FString& QuickMatchType)
{
	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->GetAuth();
		return;
	}

	UE_LOG(UT,Log,TEXT("QuickMatch: %s"),*QuickMatchType);
	PlayerOwner->StartQuickMatch(QuickMatchType);
}

FReply SUTMainMenu::OnBootCampClick()
{
	OpenTutorialMenu();
	return FReply::Handled();
}

void SUTMainMenu::OpenTutorialMenu()
{
	if (!TutorialPanel.IsValid())
	{
		SAssignNew(TutorialPanel,SUTUMGPanel,PlayerOwner).UMGClass(TEXT("/Game/RestrictedAssets/Tutorials/Blueprints/TutMainMenuWidget.TutMainMenuWidget_C"));
	}

	if (TutorialPanel.IsValid() && ActivePanel != TutorialPanel)
	{
		ActivatePanel(TutorialPanel);
	}
}


FReply SUTMainMenu::OnYourReplaysClick()
{
	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline( TEXT( "" ), TEXT( "" ) );
		return FReply::Handled();
	}

	TSharedPtr<class SUTReplayBrowserPanel> ReplayBrowser = PlayerOwner->GetReplayBrowser();
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

FReply SUTMainMenu::OnRecentReplaysClick()
{
	RecentReplays();
	return FReply::Handled();
}

void SUTMainMenu::RecentReplays()
{
	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline( TEXT( "" ), TEXT( "" ) );
		return;
	}

	TSharedPtr<class SUTReplayBrowserPanel> ReplayBrowser = PlayerOwner->GetReplayBrowser();
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

FReply SUTMainMenu::OnLiveGameReplaysClick()
{
	ShowLiveGameReplays();

	return FReply::Handled();
}

void SUTMainMenu::ShowLiveGameReplays()
{
	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline( TEXT( "" ), TEXT( "" ) );
		return;
	}

	TSharedPtr<class SUTReplayBrowserPanel> ReplayBrowser = PlayerOwner->GetReplayBrowser();
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
}

FReply SUTMainMenu::OnCommunityClick()
{
	ShowCommunity();
	return FReply::Handled();
}

void SUTMainMenu::ShowCommunity()
{
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

bool SUTMainMenu::ShouldShowBrowserIcon()
{
	return (PlayerOwner.IsValid() && PlayerOwner->bShowBrowserIconOnMainMenu);
}

void SUTMainMenu::CheckLocalContentForLanPlay()
{
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (!UTEngine->IsCloudAndLocalContentInSync())
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("SUTCreateGamePanel", "CloudSyncErrorCaption", "Cloud Not Synced"), NSLOCTEXT("SUTCreateGamePanel", "CloudSyncErrorMsg", "Some files are not up to date on your cloud storage. Would you like to start anyway?"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUTMainMenu::CloudOutOfSyncResult));
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
						NSLOCTEXT("SUTCreateGamePanel", "LocalNetworkWarningTitle", "Local Network Detected"),
						NSLOCTEXT("SUTCreateGamePanel", "LocalNetworkWarningDesc", "Make sure ports 7777 and 15000 are forwarded in your router to be visible to players outside your local network"),
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
			Callback.BindSP(this, &SUTMainMenu::StartGameWarningComplete);
			PlayerOwner->ShowMessage(NSLOCTEXT("U1T", "ContentNotInSync", "Custom Content Out of Sync"), DialogText, UTDIALOG_BUTTON_YES | UTDIALOG_BUTTON_NO, Callback);
		}
		else
		{
			StartGame(true);
		}
	}
}

void SUTMainMenu::StartGameWarningComplete(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES || ButtonID == UTDIALOG_BUTTON_OK)
	{
		StartGame(true);
	}
}

void SUTMainMenu::StartGame(bool bLanGame)
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
			OptimalPlayerCount = FMath::Min(OptimalPlayerCount, CurrentRule->MaxPlayers);
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
		FString Options = FString::Printf(TEXT("unrealtournament %s -log -server -LAN"), *URL);

		IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
		if (OnlineSubsystem)
		{
			IOnlineIdentityPtr OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
			if (OnlineIdentityInterface.IsValid())
			{
				TSharedPtr<const FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(PlayerOwner->GetControllerId());
				if (UserId.IsValid())
				{
					Options += FString::Printf(TEXT(" -cloudID=%s"), *UserId->ToString());
				}
			}
		}

		FString AppName = GetEpicAppName();
		if (!AppName.IsEmpty())
		{
			Options += FString::Printf(TEXT(" -EPICAPP=%s"), *AppName);
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


void SUTMainMenu::CloudOutOfSyncResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		StartGame(true);
	}
}

FReply SUTMainMenu::OnShowServerBrowserPanel()
{
	return SUTMenuBase::OnShowServerBrowserPanel();

}

void SUTMainMenu::OnMenuOpened(const FString& Parameters)
{
	SUTMenuBase::OnMenuOpened(Parameters);
	if (Parameters.Equals(TEXT("showchallenge"), ESearchCase::IgnoreCase))
	{
		ShowGamePanel();
	}
}

void SUTMainMenu::OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	if (NewStatus == ELoginStatus::LoggedIn)
	{
		if (TutorialPanel.IsValid() && ActivePanel == TutorialPanel)
		{
			ShowGamePanel();
			PlayerOwner->ShowMessage(
				NSLOCTEXT("SUTMainMenu", "TutorialErrorTitle", "User Changed"),
				NSLOCTEXT("SUTMainMenu", "TutorialErrorMessage", "User changed. Returning to the main menu."), UTDIALOG_BUTTON_OK, nullptr
				);
		}
	}

	SUTMenuBase::OnOwnerLoginStatusChanged(LocalPlayerOwner, NewStatus, UniqueID);
}

#endif