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
#include "Panels/SUWStatsViewer.h"
#include "Panels/SUWCreditsPanel.h"
#include "UTEpicDefaultRulesets.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

#if !UE_SERVER

void SUWindowsMainMenu::CreateDesktop()
{
	bNeedToShowGamePanel = false;
	SUTMenuBase::CreateDesktop();
}

void SUWindowsMainMenu::SetInitialPanel()
{
	SAssignNew(HomePanel, SUHomePanel, PlayerOwner);
	
	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
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

	}
}

TSharedRef<SWidget> SUWindowsMainMenu::BuildTutorialSubMenu()
{

	TSharedPtr<SComboButton> DropDownButton = NULL;
	SNew(SBox)
	.HeightOverride(56)
	[
		SAssignNew(DropDownButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button")
		.ContentPadding(FMargin(35.0,0.0,35.0,0.0))
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_TUTORIAL", "LEARN").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
			]
		]
	];

	DropDownButton->SetMenuContent
	(
		SNew(SBorder)
		.BorderImage(SUWindowsStyle::Get().GetBrush("UT.ContextMenu.Background"))
		.Padding(FMargin(0.0f,2.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Tutorial_LeanHoToPlay", "Basic Training").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUWindowsMainMenu::OnBootCampClick, DropDownButton)
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("UT.ContextMenu.Spacer"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Tutorial_Community", "Training Videos").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUWindowsMainMenu::OnCommunityClick, DropDownButton)
			]
		]
	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();

}


TSharedRef<SWidget> SUWindowsMainMenu::AddPlayNow()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SNew(SBox)
	.HeightOverride(56)
	[
		SAssignNew(DropDownButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button")
		.ContentPadding(FMargin(35.0,0.0,35.0,0.0))
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch", "PLAY").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
			]
		]
	];

	DropDownButton->SetMenuContent
	(
		SNew( SBorder )
		.BorderImage( SUWindowsStyle::Get().GetBrush("UT.ContextMenu.Background") )
		.Padding(0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f))
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_PlayDM", "QuickPlay Deathmatch").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUWindowsMainMenu::OnPlayQuickMatch, DropDownButton, QuickMatchTypes::Deathmatch)
			]
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f))
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_PlayCTF", "QuickPlay Capture the Flag").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUWindowsMainMenu::OnPlayQuickMatch, DropDownButton, QuickMatchTypes::CaptureTheFlag)
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.AutoHeight()
			.Padding(FMargin(0.0f, 0.0f))
			[
				SNew(SBox)
				.HeightOverride(16)
				[
					SNew(SImage )
					.Image( SUWindowsStyle::Get().GetBrush("UT.ContextMenu.Spacer") )
				]
			]
		 
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f))
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_CreateGame", "Create a Game").ToString())
				.OnClicked(this, &SUWindowsMainMenu::OnShowGamePanel, DropDownButton)
			]
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(16)
				[
					SNew(SImage )
					.Image( SUWindowsStyle::Get().GetBrush("UT.ContextMenu.Spacer") )
				]
			]
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f))
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_FindGame", "Find a Game...").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTMenuBase::OnShowServerBrowser, DropDownButton)
			]
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.0f))
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_IPConnect", "Connect via IP").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUWindowsMainMenu::OnConnectIP, DropDownButton)
			]
		]
	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();
}

FReply SUWindowsMainMenu::OnCloseClicked()
{
	PlayerOwner->HideMenu();
	ConsoleCommand(TEXT("quit"));
	return FReply::Handled();
}



FReply SUWindowsMainMenu::OnShowGamePanel(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) 
	{
		MenuButton->SetIsOpen(false);
	}

	if (TickCountDown <= 0)
	{
		if (GamePanel.IsValid())
		{
			ActivatePanel(GamePanel);
		}
		else
		{
			PlayerOwner->ShowContentLoadingMessage();
			bNeedToShowGamePanel = true;
			TickCountDown = 3;
		}
	}

	return FReply::Handled();
}

void SUWindowsMainMenu::OpenDelayedMenu()
{
	SUTMenuBase::OpenDelayedMenu();
	if (bNeedToShowGamePanel)
	{

		if (AvailableGameRulesets.Num() == 0)
		{
			UUTEpicDefaultRulesets::GetDefaultRules(PlayerOwner->GetWorld()->GetAuthGameMode(), AvailableGameRulesets);
			for (int32 i=0; i < AvailableGameRulesets.Num(); i++)
			{
				AvailableGameRulesets[i]->BuildSlateBadge();
			}
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
	if (ButtonPressed != UTDIALOG_BUTTON_CANCEL && CreateGameDialog.IsValid() && CreateGameDialog->SelectedRuleset.IsValid() && CreateGameDialog->MapPlayList.Num() > 0)
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

FReply SUWindowsMainMenu::OnTutorialClick()
{
	ConsoleCommand(TEXT("Open " + PlayerOwner->TutorialLaunchParams));
	return FReply::Handled();
}


FReply SUWindowsMainMenu::OnPlayQuickMatch(TSharedPtr<SComboButton> MenuButton, FName QuickMatchType)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline(TEXT(""),TEXT(""));
		return FReply::Handled();
	}

	UE_LOG(UT,Log,TEXT("QuickMatch: %s"),*QuickMatchType.ToString());
	PlayerOwner->StartQuickMatch(QuickMatchType);
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnBootCampClick(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	ConsoleCommand(TEXT("start TUT-BasicTraining?timelimit=0"));
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnCommunityClick(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid())
	{
		MenuButton->SetIsOpen(false);
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
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnConnectIP(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid())
	{
		MenuButton->SetIsOpen(false);
	}
	PlayerOwner->OpenDialog(
							SNew(SUWInputBox)
							.DefaultInput(PlayerOwner->LastConnectToIP)
							.DialogSize(FVector2D(600, 200))
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
				if (LP.IsValid() && LP->PlayerController)
				{
					LP->PlayerController->bShowMouseCursor = false;
					LP->PlayerController->SetInputMode(FInputModeGameOnly());
				}

				UUTGameUserSettings* GameSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
				GameSettings->SaveConfig();
			};

			bool bCanBindAll = false;
			TSharedRef<FInternetAddr> Address = ISocketSubsystem::Get()->GetLocalHostAddr(*GWarn, bCanBindAll);
			if (Address->IsValid())
			{
				FString StringAddress = Address->ToString(false);

				// Note: This is an extremely basic way to test for local network and it only covers the common case
				if (StringAddress.StartsWith(TEXT("192.168.")))
				{
					if( PlayerOwner->PlayerController )
					{
						PlayerOwner->PlayerController->SetInputMode( FInputModeUIOnly() );
					}
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

		if (PlayerOwner->PlayerController && bShowingLanWarning)
		{ 
			// ensure the user can click the warning.  The game will have tried to hide the cursor otherwise
			PlayerOwner->PlayerController->bShowMouseCursor = true;
			PlayerOwner->PlayerController->SetInputMode(FInputModeUIOnly());
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

	// Build the URL

	// First copy the map play list. TODO: Find a good solution for the map list without using the ini if possible
	PlayerOwner->SinglePlayerMapList.Empty();
	for (int32 i=0; i< CreateGameDialog->MapPlayList.Num(); i++ )
	{
		if (CreateGameDialog->MapPlayList[i].bSelected)
		{
			PlayerOwner->SinglePlayerMapList.Add(CreateGameDialog->MapPlayList[i].MapName);
		}
	}

	// First, grab the starting map.
	FString URL = PlayerOwner->SinglePlayerMapList[0] + FString::Printf(TEXT("?Game=%s"), *CreateGameDialog->SelectedRuleset->GameMode);
	URL += CreateGameDialog->SelectedRuleset->GameOptions;

	// Set the Max players
	URL += FString::Printf(TEXT("?MaxPlayers=%i"), CreateGameDialog->SelectedRuleset->MaxPlayers);

	// Set the Bot Skill level and if they are needed.
	if (CreateGameDialog->BotSkillLevel >= 0)
	{
		URL += FString::Printf(TEXT("?BotFill=%i?Difficulty=%i"), CreateGameDialog->SelectedRuleset->MaxPlayers, FMath::Clamp<int32>(CreateGameDialog->BotSkillLevel,0,7));
	}

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
}


void SUWindowsMainMenu::CloudOutOfSyncResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		StartGame(true);
	}
}


#endif