// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTMenuBase.h"
#include "../Panels/SUTCreateGamePanel.h"
#include "../Panels/SUTHomePanel.h"
#include "../Panels/SUTWebBrowserPanel.h"
#include "UTReplicatedGameRuleset.h"

const FString CommunityVideoURL = "http://epic.gm/utlaunchertutorial";

class SUTGameSetupDialog;
class SUTFragCenterPanel;
class SUTHomePanel;
class SUTChallengePanel;
class SUTUMGPanel;

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTMainMenu : public SUTMenuBase
{
protected:
	bool bNeedToShowGamePanel;

	TSharedPtr<SUTCreateGamePanel> GamePanel;
	TSharedPtr<SUTWebBrowserPanel> WebPanel;
	
	TSharedRef<SWidget> AddPlayNow();

	virtual void CreateDesktop();
	virtual TSharedRef<SWidget> BuildBackground();
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
	TSharedPtr<SUTGameSetupDialog> CreateGameDialog;
	void OnGameChangeDialogResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed);

	virtual void CheckLocalContentForLanPlay();
	virtual void CloudOutOfSyncResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void StartGameWarningComplete(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID);
	virtual void StartGame(bool bLanGame);

	virtual FReply OnShowHomePanel() override;

	virtual FReply OnFragCenterClick();

	TSharedPtr<SUTFragCenterPanel> FragCenterPanel;
	TSharedPtr<SUTChallengePanel> ChallengePanel;
	TSharedPtr<SUTUMGPanel> TutorialPanel;

	virtual void OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID) override;

public:
	virtual ~SUTMainMenu();

	virtual FReply OnShowServerBrowserPanel();

	virtual void ShowGamePanel();
	virtual void ShowCustomGamePanel();
	virtual void ShowCommunity();
	virtual void ShowFragCenter();
	virtual void OpenTutorialMenu();
	virtual void RecentReplays();
	virtual void ShowLiveGameReplays();
	virtual void QuickPlay(const FString& QuickMatchType);
	virtual void DeactivatePanel(TSharedPtr<class SUTPanelBase> PanelToDeactivate);

	virtual void OnMenuOpened(const FString& Parameters);



};
#endif
