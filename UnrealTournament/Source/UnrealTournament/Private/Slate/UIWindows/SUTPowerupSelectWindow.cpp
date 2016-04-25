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

void SUTPowerupSelectWindow::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner, const FString& BlueprintPath)
{
	WidgetString = BlueprintPath;

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
		Overlay->AddSlot().VAlign(VAlign_Fill).HAlign(HAlign_Fill)
		[
			SAssignNew(PowerupSelectPanel, SUTUMGPanel, PlayerOwner)
			.UMGClass(WidgetString)
		];
	}
}

#endif