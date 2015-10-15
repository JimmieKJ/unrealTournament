// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTMenuBase.h"
#include "Widgets/SUTComboButton.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTInGameMenu : public SUTMenuBase
{
public:
	virtual FReply OnReturnToLobby();
	virtual FReply OnReturnToMainMenu();

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
};
#endif
