// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "SlateBasics.h"
#include "SUTInGameMenu.h"
#include "Panels/SUMidGameInfoPanel.h"
#include "Panels/SULobbyInfoPanel.h"
#include "Slate/SlateGameResources.h"

#if !UE_SERVER

class SULobbyInfoPanel;

class UNREALTOURNAMENT_API SUWindowsLobby : public SUTInGameMenu
{
	FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent);

protected:
	
	TSharedPtr<SButton> MatchButton;
	TSharedPtr<SULobbyInfoPanel> InfoPanel;

	virtual void SetInitialPanel();

	virtual FText GetDisconnectButtonText() const;


	FText GetMatchButtonText() const;
	FString GetMatchCount() const;

	FReply MatchButtonClicked();
	
	virtual void BuildExitMenu(TSharedPtr <SUTComboButton> ExitButton);
	virtual TSharedRef<SWidget> BuildBackground();
	virtual TSharedRef<SWidget> BuildOptionsSubMenu();
	FReply OpenHUDSettings();
};

#endif