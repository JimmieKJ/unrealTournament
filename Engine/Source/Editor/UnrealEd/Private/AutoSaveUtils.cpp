// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutoSaveUtils.h"
#include "Misc/Paths.h"

FString AutoSaveUtils::GetAutoSaveDir()
{
	return FPaths::GameSavedDir() / TEXT("Autosaves");
}
