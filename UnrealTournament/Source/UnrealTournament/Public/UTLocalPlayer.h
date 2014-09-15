// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "../Private/Slate/SUWDialog.h"
#include "UTLocalPlayer.generated.h"

UCLASS()
class UUTLocalPlayer : public ULocalPlayer
{
	GENERATED_UCLASS_BODY()

public:
	virtual FString GetNickname() const;
	virtual void PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID);

	virtual void ShowMenu();
	virtual void HideMenu();

#if !UE_SERVER
	virtual TSharedPtr<class SUWDialog> ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate());

	/** utilities for opening and closing dialogs */
	virtual void OpenDialog(TSharedRef<class SUWDialog> Dialog);
	virtual void CloseDialog(TSharedRef<class SUWDialog> Dialog);
#endif

	virtual bool IsMenuGame();

#if !UE_SERVER
	TSharedPtr<class SUWServerBrowser> GetServerBrowser();
#endif

protected:

#if !UE_SERVER
	TSharedPtr<class SUWindowsDesktop> DesktopSlateWidget;
	
	// Holds a persistent reference to the server browser.
	TSharedPtr<class SUWServerBrowser> ServerBrowserWidget;

	/** stores a reference to open dialogs so they don't get destroyed */
	TArray< TSharedPtr<class SUWDialog> > OpenDialogs;
#endif

};




