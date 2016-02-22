// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *	This is the base class for our menus.  UT Menus have two components.  A horz. menu bar across the top 
 *  and panels that can be activated in the space below it.  See SUTPanelBase for more information on the panels
 **/

#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Widgets/SUTComboButton.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTMenuBase : public SCompoundWidget
{

	SLATE_BEGIN_ARGS(SUTMenuBase)
	{}

	SLATE_ARGUMENT(UUTLocalPlayer*, PlayerOwner)

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	/**
	 *	Will be called from the Local Player when the menu has been opened
	 **/
	virtual void OnMenuOpened(const FString& Parameters);
	virtual void OnMenuClosed();
	virtual void CloseMenus();

	virtual FReply OnMenuConsoleCommand(FString Command);
	virtual void ConsoleCommand(FString Command);

	virtual void ActivatePanel(TSharedPtr<class SUTPanelBase> PanelToActivate);
	virtual void DeactivatePanel(TSharedPtr<class SUTPanelBase> PanelToDeactivate);

	// Called from the child when the panel decides it's hidden.  Useful for out animations.
	virtual void PanelHidden(TSharedPtr<SWidget> Child);
	virtual void ShowHomePanel();

protected:

	int32 ZOrderIndex;

	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;

	TSharedPtr<class SOverlay> Desktop;
	TSharedPtr<class SUTPanelBase> ActivePanel;
	TSharedPtr<class SWidget> GameViewportWidget;

	virtual void CreateDesktop();

	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent ) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual void OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

private:
	FDelegateHandle PlayerOnlineStatusChangedDelegate;
	virtual void OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);


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

	virtual TSharedPtr<SUTPanelBase> GetActivePanel();

protected:
	TSharedPtr<class SHorizontalBox> LeftMenuBar;
	TSharedPtr<class SHorizontalBox> RightMenuBar;
	TArray< TSharedPtr<SUTComboButton> > MenuButtons;

	TSharedPtr<SUTPanelBase> HomePanel;

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

	const FSlateBrush* GetFullvsWindowButtonImage() const;
	FReply ToggleFullscreenClicked();
	FReply MinimizeClicked();

	virtual EVisibility GetBackVis() const;

	FReply ExitClicked();
	virtual void QuitConfirmation();
	virtual void QuitConfirmationResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	const FSlateBrush* GetFocusBrush() const
	{
		return FCoreStyle::Get().GetBrush("NoBrush");
	}



public:
	virtual bool SkipWorldRender();

};
#endif
