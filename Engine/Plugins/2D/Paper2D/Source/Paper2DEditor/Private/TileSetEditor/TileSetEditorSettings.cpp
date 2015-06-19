// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "TileSetEditorSettings.h"

//////////////////////////////////////////////////////////////////////////
// UTileSetEditorSettings

UTileSetEditorSettings::UTileSetEditorSettings()
	: DefaultBackgroundColor(0, 0, 127)
	, bShowGridByDefault(true)
	, ExtrusionAmount(2)
	, bPadToPowerOf2(true)
	, bFillWithTransparentBlack(true)
{
}
