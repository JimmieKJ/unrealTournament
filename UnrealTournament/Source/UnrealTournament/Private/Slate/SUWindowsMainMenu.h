// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTMenuBase.h"
#include "Panels/SUWCreateGamePanel.h"
#include "Panels/SUHomePanel.h"
#include "Panels/SUTWebBrowserPanel.h"
#include "UTReplicatedGameRuleset.h"

const FString CommunityVideoURL = "http://epic.gm/utlaunchertutorial";

class SUWGameSetupDialog;
class SUTFragCenterPanel;
class SUTHomePanel;

#if !UE_SERVER
class UNREALTOURNAMENT_API SUWindowsMainMenu : public SUTMenuBase
{
protected:
	bool bNeedToShowGamePanel;

	TSharedPtr<SUWCreateGamePanel> GamePanel;
	TSharedPtr<SUTWebBrowserPanel> WebPanel;
	
	TSharedRef<SWidget> AddPlayNow();

	virtual void CreateDesktop();
	virtual void SetInitialPanel();

	virtual void BuildLeftMenuBar();

	virtual TSharedRef<SWidget> BuildWatchSubMenu();
	virtual TSharedRef<SWidget> BuildTutorialSubMenu();

	virtual FReply OnShowGamePanel(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnCloseClicked();

	virtual FReply OnYourReplaysClick(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnRecentReplaysClick(TSharedPtr<SComboButton> MenuButton);

	virtual FReply OnLiveGameReplaysClick(TSharedPtr<SComboButton> MenuButton);

	virtual FReply OnBootCampClick(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnCommunityClick(TSharedPtr<SComboButton> MenuButton);

	virtual FReply OnPlayQuickMatch(TSharedPtr<SComboButton> MenuButton, FString QuickMatchType);

	virtual FReply OnConnectIP(TSharedPtr<SComboButton> MenuButton);
	virtual void ConnectIPDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	virtual void OpenDelayedMenu();
	virtual bool ShouldShowBrowserIcon();

	TArray<TWeakObjectPtr<AUTReplicatedGameRuleset>> AvailableGameRulesets;
	TSharedPtr<SUWGameSetupDialog> CreateGameDialog;
	void OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed);

	virtual void CheckLocalContentForLanPlay();
	virtual void CloudOutOfSyncResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void StartGameWarningComplete(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID);
	virtual void StartGame(bool bLanGame);

	virtual FReply OnShowHomePanel() override;

	virtual FReply OnFragCenterClick(TSharedPtr<SComboButton> MenuButton);

	TSharedPtr<SUTFragCenterPanel> FragCenterPanel;
	TWeakObjectPtr<class UUserWidget> TutorialMenu;

public:
	virtual ~SUWindowsMainMenu();

	virtual FReply OnShowServerBrowserPanel();

	virtual void ShowGamePanel();
	virtual void ShowCommunity();
	virtual void ShowFragCenter();
	virtual void OpenTutorialMenu();
	virtual void RecentReplays();
	virtual void ShowLiveGameReplays();
	virtual void QuickPlay(const FString& QuickMatchType);
	virtual void DeactivatePanel(TSharedPtr<class SUWPanel> PanelToDeactivate);
};
#endif
