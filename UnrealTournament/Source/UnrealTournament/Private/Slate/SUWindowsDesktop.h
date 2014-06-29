// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "Slate/SlateGameResources.h"

class SUWindowsDesktop : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWindowsDesktop)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner)

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	/**
	 *	Will be called from the Local Player when the menu has been opened
	 **/
	virtual void OnMenuOpened();
	virtual void OnMenuClosed();
	virtual void CloseMenus();


private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;

	TSharedPtr<class SWidget> GameViewportWidget;
	TSharedPtr<class SHorizontalBox> MenuBar;


	virtual FReply OnMenuConsoleCommand(FString Command);
	virtual FReply OnChangeTeam(int32 NewTeamIndex);

	virtual bool SupportsKeyboardFocus() const OVERRIDE;
	virtual FReply OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent ) OVERRIDE;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent ) OVERRIDE;

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) OVERRIDE;

	virtual TSharedRef<SWidget> BuildMenuBar();

	virtual void BuildFileSubMenu();
	virtual void BuildGameSubMenu();
	virtual void BuildOptionsSubMenu();
};

