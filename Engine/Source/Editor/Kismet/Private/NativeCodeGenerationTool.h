// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditor.h"

//////////////////////////////////////////////////////////////////////////
// FNativeCodeGenerationTool

struct FNativeCodeGenerationTool
{
	static void Open(UBlueprint& Object, TSharedRef< class FBlueprintEditor> Editor);

	static bool CanGenerate(const UBlueprint& Object);
};