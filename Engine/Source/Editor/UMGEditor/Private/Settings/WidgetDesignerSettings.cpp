// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "Settings/WidgetDesignerSettings.h"

UWidgetDesignerSettings::UWidgetDesignerSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GridSnapEnabled = true;
	GridSnapSize = 4;
	bShowOutlines = true;
}
