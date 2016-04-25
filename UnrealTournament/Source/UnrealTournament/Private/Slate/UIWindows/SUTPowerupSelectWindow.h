// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"

#include "SUTHUDWindow.h"
#include "SUTUMGPanel.h"

#include "UTPowerupSelectorUserWidget.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTPowerupSelectWindow : public SUTHUDWindow
{
	virtual bool CanWindowClose();

	virtual FName IdentifyWindow()
	{
		return FName(TEXT("PowerupSelectWindow"));
	}

	// Returns the input mode needed if this window is active
	virtual EInputMode::Type GetInputMode()
	{
		return EInputMode::EIM_UIOnly;
	}


public:

	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner, const FString& BlueprintPath);

	bool bShouldShowDefensePowerupSelectWindow;


protected:
	virtual void BuildWindow();
	virtual void BuildPowerupSelect();

	TSharedPtr<SUTUMGPanel> PowerupSelectPanel;
	FString WidgetString;
};

#endif
