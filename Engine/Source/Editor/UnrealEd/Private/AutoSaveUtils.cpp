// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AutoSaveUtils.h"

FString AutoSaveUtils::GetAutoSaveDir()
{
	return FPaths::GameSavedDir() / TEXT("Autosaves");
}
