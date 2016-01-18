
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../Widgets/SUTScaleBox.h"
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