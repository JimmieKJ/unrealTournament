// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTPowerupSelectWindow.h"
#include "../SUWindowsStyle.h"
#include "../Widgets/SUTButton.h"
#include "../SUTUtils.h"
#include "UTPowerupSelectorUserWidget.h"
#include "UTPlayerState.h"

#if !UE_SERVER

void SUTPowerupSelectWindow::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner, bool bShouldShowDefensePowerupSelectWindowIn)
{
	bShouldShowDefensePowerupSelectWindow = bShouldShowDefensePowerupSelectWindowIn;

	SUTHUDWindow::Construct(InArgs, InPlayerOwner);
}

bool SUTPowerupSelectWindow::CanWindowClose()
{
	return true;
}

void SUTPowerupSelectWindow::BuildWindow()
{
	SUTHUDWindow::BuildWindow();
	BuildPowerupSelect();
}

void SUTPowerupSelectWindow::BuildPowerupSelect()
{
	if (!PowerupSelectPanel.IsValid())
	{
		FString WidgetString;
		WidgetString = TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerupSelector_Base_UserWidget.BP_PowerupSelector_Base_UserWidget_C");

		//Needed if we change to Defense and Offense specific powerups or layouts
		/*if (bShouldShowDefensePowerupSelectWindow)
		{
			WidgetString = TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerSelector_Defense_UserWidget.BP_PowerSelector_Defense_UserWidget_C");
		}*/
		
		Overlay->AddSlot().VAlign(VAlign_Fill).HAlign(HAlign_Fill)
		[
			SAssignNew(PowerupSelectPanel, SUTUMGPanel, PlayerOwner)
			.UMGClass(WidgetString)
		];
	}
}

#endif