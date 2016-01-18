// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditor.h"

//////////////////////////////////////////////////////////////////////////
// FNativeCodeGenerationTool

struct FNativeCodeGenerationTool
{
	BLUEPRINTNATIVECODEGEN_API static void Open(UBlueprint& Object, TSharedRef< class FBlueprintEditor> Editor);
	BLUEPRINTNATIVECODEGEN_API static bool CanGenerate(const UBlueprint& Object);
};