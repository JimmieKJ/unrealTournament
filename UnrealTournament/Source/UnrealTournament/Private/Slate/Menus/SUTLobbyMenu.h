// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "SlateBasics.h"
#include "SUTInGameMenu.h"
#include "../Panels/SUTMidGameInfoPanel.h"
#include "../Panels/SUTLobbyInfoPanel.h"
#include "Slate/SlateGameResources.h"

#if !UE_SERVER

class SUTLobbyInfoPanel;

class UNREALTOURNAMENT_API SUTLobbyMenu : public SUTInGameMenu
{
	FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent);

protected:
	
	TSharedPtr<SButton> MatchButton;
	TSharedPtr<SUTLobbyInfoPanel> InfoPanel;

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