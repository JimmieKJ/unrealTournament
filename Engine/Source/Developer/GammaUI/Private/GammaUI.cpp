// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GammaUIPrivatePCH.h"
#include "GammaUIPanel.h"
#include "ModuleManager.h"

IMPLEMENT_MODULE( FGammaUI, GammaUI );

void FGammaUI::StartupModule()
{
}

void FGammaUI::ShutdownModule()
{
}

/** @return a pointer to the Gamma manager panel; invalid pointer if one has already been allocated */
TSharedPtr< SWidget > FGammaUI::GetGammaUIPanel()
{
	return SNew(SGammaUIPanel);
}
