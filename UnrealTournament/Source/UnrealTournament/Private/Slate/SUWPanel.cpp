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
	BuildPage(ViewportSize);
}

void SUWPanel::BuildPage(FVector2D ViewportSize){}
void SUWPanel::OnShowPanel()
{
}
void SUWPanel::OnHidePanel()
{
}


#endif