// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

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

	virtual TSharedPtr<class SUWDialog> ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate());

	/** utilities for opening and closing dialogs */
	virtual void OpenDialog(TSharedRef<class SUWDialog> Dialog);
	virtual void CloseDialog(TSharedRef<class SUWDialog> Dialog);

protected:

	TSharedPtr<class SUWindowsDesktop> DesktopSlateWidget;	

	/** stores a reference to open dialogs so they don't get destroyed */
	TArray< TSharedPtr<class SUWDialog> > OpenDialogs;
};




