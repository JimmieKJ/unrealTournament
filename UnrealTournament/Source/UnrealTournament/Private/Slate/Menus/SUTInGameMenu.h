// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTMenuBase.h"
#include "../Widgets/SUTComboButton.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTInGameMenu : public SUTMenuBase
{
public:
	virtual FReply OnReturnToLobby();
	virtual FReply OnReturnToMainMenu();

	virtual void OnMenuOpened(const FString& Parameters);

protected:

	virtual void BuildLeftMenuBar();
	virtual void BuildExitMenu(TSharedPtr <SUTComboButton> ExitButton);

	virtual FReply OnCloseMenu();

	virtual FReply OnTeamChangeClick();
	virtual FReply OnReadyChangeClick();
	virtual FReply OnMapVoteClick();
	virtual FReply OnSpectateClick();
	virtual void SetInitialPanel();
	
	virtual FReply OpenHUDSettings();
	virtual FText GetMapVoteTitle() const;
	virtual void WriteQuitMidGameAnalytics();

	virtual void ShowExitDestinationMenu();
	virtual void QuitConfirmation();
	void OnDestinationResult(int32 PickedIndex);
	void ShowHomePanel();
	void BackResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed);

	EVisibility GetChangeTeamVisibility() const;
	EVisibility GetMatchSummaryVisibility() const;

	FReply ShowSummary();
};
#endif
