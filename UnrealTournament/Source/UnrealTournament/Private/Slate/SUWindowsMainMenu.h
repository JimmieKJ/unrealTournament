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
class SUTChallengePanel;

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

	virtual FReply OnCloseClicked();

	virtual FReply OnYourReplaysClick();
	virtual FReply OnRecentReplaysClick();

	virtual FReply OnLiveGameReplaysClick();

	virtual FReply OnBootCampClick();
	virtual FReply OnCommunityClick();


	virtual FReply OnPlayQuickMatch(FString QuickMatchType);
	virtual FReply OnShowGamePanel();
	virtual FReply OnShowCustomGamePanel();
		
	virtual void OpenDelayedMenu();
	virtual bool ShouldShowBrowserIcon();

	TArray<AUTReplicatedGameRuleset*> AvailableGameRulesets;
	TSharedPtr<SUWGameSetupDialog> CreateGameDialog;
	void OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed);

	virtual void CheckLocalContentForLanPlay();
	virtual void CloudOutOfSyncResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void StartGameWarningComplete(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID);
	virtual void StartGame(bool bLanGame);

	virtual FReply OnShowHomePanel() override;

	virtual FReply OnFragCenterClick();

	TSharedPtr<SUTFragCenterPanel> FragCenterPanel;
	TWeakObjectPtr<class UUserWidget> TutorialMenu;

	TSharedPtr<SUTChallengePanel> ChallengePanel;

	virtual void OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID) override;

public:
	virtual ~SUWindowsMainMenu();

	virtual FReply OnShowServerBrowserPanel();

	virtual void ShowGamePanel();
	virtual void ShowCustomGamePanel();
	virtual void ShowCommunity();
	virtual void ShowFragCenter();
	virtual void OpenTutorialMenu();
	virtual void RecentReplays();
	virtual void ShowLiveGameReplays();
	virtual void QuickPlay(const FString& QuickMatchType);
	virtual void DeactivatePanel(TSharedPtr<class SUWPanel> PanelToDeactivate);

	virtual void OnMenuOpened(const FString& Parameters);

};
#endif
