// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SUWindowsMainMenu.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTReplayMenu : public SUWindowsMainMenu
{
	//Disable the frag center
	virtual void SetInitialPanel() override { ; }

	//Remove the play game stuff for now
	virtual void BuildLeftMenuBar() override;

	virtual FReply OnReturnToMainMenu(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnCloseMenu(TSharedPtr<SComboButton> MenuButton);

	virtual void BuildExitMenu(TSharedPtr <SComboButton> ExitButton, TSharedPtr<SVerticalBox> MenuSpace) override;
};
#endif
