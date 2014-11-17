// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWPanel.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

void SUWPanel::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	checkSlow(PlayerOwner != NULL);

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);
	ConstructPanel(ViewportSize);
}

void SUWPanel::ConstructPanel(FVector2D ViewportSize){}

void SUWPanel::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	ParentWindow = inParentWindow;
}
void SUWPanel::OnHidePanel()
{
	ParentWindow.Reset();
}

void SUWPanel::ConsoleCommand(FString Command)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController != NULL)
	{
		PlayerOwner->Exec(PlayerOwner->GetWorld(), *Command, *GLog);
	}
}


#endif