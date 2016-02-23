// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SUTMainMenu.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTReplayMenu : public SUTMainMenu
{
	//Disable the frag center
	virtual void SetInitialPanel() override { ; }

	//Remove the play game stuff for now
	virtual void BuildLeftMenuBar() override;

	virtual FReply OnReturnToMainMenu();
	virtual FReply OnCloseMenu();

	virtual void BuildExitMenu(TSharedPtr <SUTComboButton> ExitButton) override;

	virtual EVisibility GetBackVis() const;
	virtual FReply OnShowHomePanel();

	FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent );

protected:
	virtual TSharedRef<SWidget> BuildBackground();
};
#endif
