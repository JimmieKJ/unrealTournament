
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUWScaleBox.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "SUTFragCenterPanel.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

void SUTFragCenterPanel::ConstructPanel(FVector2D ViewportSize)
{
	SUTWebBrowserPanel::ConstructPanel(ViewportSize);
}


#endif