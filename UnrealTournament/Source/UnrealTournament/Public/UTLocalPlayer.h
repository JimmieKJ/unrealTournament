// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLocalPlayer.generated.h"

/** Defines the current state of the game. */


UCLASS()
class UUTLocalPlayer : public ULocalPlayer
{
	GENERATED_UCLASS_BODY()

public:
	virtual FString GetNickname() const;
	virtual void PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID);

	virtual void ShowMenu();
	virtual void HideMenu();

	virtual void ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons,  UObject* Host, FName ResultFunction);
	virtual void MessageBoxDialogResult(uint16 ButtonID);

protected:

	FDialogResultDelegate OnDialogResult;

	TSharedPtr<class SUWindowsDesktop> DesktopSlateWidget;	
	TSharedPtr<class SUWMessageBox> MessageBoxWidget;
};




