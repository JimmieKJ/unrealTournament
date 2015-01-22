// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	LightingTools.cpp: Lighting Tools helper
================================================================================*/

#include "LevelEditor.h"
#include "LightingTools.h"
#include "EditorSupportDelegates.h"

/**
 *	LightingTools settings
 */
/** Static: Global lighting tools adjust settings */
FLightingToolsSettings FLightingToolsSettings::LightingToolsSettings;

void FLightingToolsSettings::Init()
{
	FLightingToolsSettings& Settings = Get();
	for (int32 ViewIndex = 0; ViewIndex < GEditor->AllViewportClients.Num(); ViewIndex++)
	{
		if (GEditor->AllViewportClients[ViewIndex]->IsPerspective())
		{
			Settings.bSavedShowSelection = GEditor->AllViewportClients[ViewIndex]->EngineShowFlags.Selection;
			GEditor->AllViewportClients[ViewIndex]->EngineShowFlags.Selection = 0;
			break;
		}
	}
	ApplyToggle();
}

bool FLightingToolsSettings::ApplyToggle()
{
	FLightingToolsSettings& Settings = Get();

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

	return true;
}

void FLightingToolsSettings::Reset()
{
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

	FLightingToolsSettings& Settings = Get();
	if (Settings.bSavedShowSelection)
	{
		for (int32 ViewIndex = 0; ViewIndex < GEditor->AllViewportClients.Num(); ViewIndex++)
		{
			if (GEditor->AllViewportClients[ViewIndex]->IsPerspective())
			{
				GEditor->AllViewportClients[ViewIndex]->EngineShowFlags.Selection = 1;
				break;
			}
		}
	}
}
