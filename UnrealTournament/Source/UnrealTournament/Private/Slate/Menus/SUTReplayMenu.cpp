// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTReplayMenu.h"
#include "../Panels/SUTReplayBrowserPanel.h"

#if !UE_SERVER

/****************************** [ Build Sub Menus ] *****************************************/

void SUTReplayMenu::BuildLeftMenuBar()
{
	if (LeftMenuBar.IsValid())
	{
		LeftMenuBar->AddSlot()
		.Padding(40.0f, 0.0f, 0.0f, 0.0f)
		.AutoWidth()
		[
			BuildWatchSubMenu()
		];
	}
}


void SUTReplayMenu::BuildExitMenu(TSharedPtr<SUTComboButton> ExitButton)
{
	ExitButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Exit_ReturnToGame", "Close Menu"), FOnClicked::CreateSP(this, &SUTReplayMenu::OnCloseMenu));
	ExitButton->AddSpacer();
	ExitButton->AddSubMenuItem(NSLOCTEXT("SUTInGameMenu", "MenuBar_ReturnToMainMenu", "Return to Main Menu"), FOnClicked::CreateSP(this, &SUTReplayMenu::OnReturnToMainMenu));
}

FReply SUTReplayMenu::OnCloseMenu()
{
	CloseMenus();

	return FReply::Handled();
}

FReply SUTReplayMenu::OnReturnToMainMenu()
{
	CloseMenus();
	PlayerOwner->ReturnToMainMenu();
	return FReply::Handled();
}

EVisibility SUTReplayMenu::GetBackVis() const
{
	return EVisibility::Visible;
}

FReply SUTReplayMenu::OnShowHomePanel()
{
	return OnReturnToMainMenu();
}

TSharedRef<SWidget> SUTReplayMenu::BuildBackground()
{
	return SNew(SCanvas);
}

FReply SUTReplayMenu::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent )
{
	PlayerOwner->GetSlateOperations() = FReply::Handled().ReleaseMouseCapture().SetUserFocus(HomeButton.ToSharedRef(), EFocusCause::SetDirectly);
	return FReply::Handled().ReleaseMouseCapture();
}



#endif