// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"
#include "UMGEditorProjectSettings.h"

UUMGEditorProjectSettings::UUMGEditorProjectSettings(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	bShowWidgetsFromEngineContent = false;
	bShowWidgetsFromDeveloperContent = true;
}