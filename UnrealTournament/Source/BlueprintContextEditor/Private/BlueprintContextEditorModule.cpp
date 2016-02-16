// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContextEditor.h"

#include "ModuleManager.h"

DEFINE_LOG_CATEGORY( LogBlueprintContextEditor );

class FBlueprintContextEditorModule : public IBlueprintContextEditorModule
{
public:
	FBlueprintContextEditorModule( ) {}
};

IMPLEMENT_MODULE( FBlueprintContextEditorModule, BlueprintContextEditor );