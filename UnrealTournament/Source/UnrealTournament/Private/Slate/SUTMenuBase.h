// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "Widgets/SUTComboButton.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTMenuBase : public SUWindowsDesktop
{
public:
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual FReply OnShowServerBrowser();

	virtual FReply OpenControlSettings();
	virtual FReply OpenPlayerSettings();
	virtual FReply OpenSocialSettings();
	virtual FReply OpenWeaponSettings();
	virtual FReply OpenSystemSettings();
	virtual FReply OpenHUDSettings();
	virtual FReply OpenProfileItems();
	virtual FReply ClearCloud();

	virtual FReply OnShowServerBrowserPanel();

	virtual void ShowHomePanel();

protected:
	TSharedPtr<class SHorizontalBox> LeftMenuBar;
	TSharedPtr<class SHorizontalBox> RightMenuBar;
	TArray< TSharedPtr<SUTComboButton> > MenuButtons;

	TSharedPtr<SUWPanel> HomePanel;

	virtual void CreateDesktop();

	virtual FReply OpenTPSReport();
	virtual FReply OpenCredits();
	virtual FReply OnMenuHTTPButton(FString URL);
	virtual FReply ShowWidgetReflector();
	virtual FReply OnOnlineClick();

	virtual void BuildLeftMenuBar();
	virtual void BuildRightMenuBar();


	virtual TSharedRef<SWidget> BuildOptionsSubMenu();
	virtual TSharedRef<SWidget> BuildAboutSubMenu();
	virtual TSharedRef<SWidget> BuildOnlinePresence();
	virtual TSharedRef<SWidget> BuildBackground();

	virtual void BuildExitMenu(TSharedPtr<SUTComboButton> ExitButton);
	
	virtual void OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

	virtual void SetInitialPanel();
	virtual FReply OnShowHomePanel();



	// The player has clicked on
	virtual FReply OnCloseClicked();
	virtual FReply ToggleFriendsAndChat();
	virtual FReply OnShowStatsViewer();
	virtual FReply OnShowPlayerCard();
	virtual FText GetBrowserButtonText() const;


private:

	// Builds out the default options for the left menu.  This includes the HOME button first and the Server Browser button last
	TSharedRef<SWidget> BuildDefaultLeftMenuBar();

	// Builds out the default options for the right menu.  Thisd includes Exit, Info and the Player Online panel
	TSharedRef<SWidget> BuildDefaultRightMenuBar();

	/** Gets the social notification state for the player - controls the ! bang on the Friends and Chat button */
	EVisibility GetSocialBangVisibility() const;

protected:
	bool bNeedsPlayerOptions;
	bool bNeedsWeaponOptions;
	bool bShowingFriends;
	
	int32 TickCountDown;
	virtual void OpenDelayedMenu();

	virtual bool ShouldShowBrowserIcon()
	{
		return true;
	}

	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent ) override;

	const FSlateBrush* GetFullvsWindowButtonImage() const;
	FReply ToggleFullscreenClicked();
	FReply MinimizeClicked();

	virtual EVisibility GetBackVis() const;

	FReply ExitClicked();
	virtual void QuitConfirmation();
	virtual void QuitConfirmationResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	FReply Logout();
	void SignOutConfirmationResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
};
#endif
