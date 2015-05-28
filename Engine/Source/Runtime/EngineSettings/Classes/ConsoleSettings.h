// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConsoleSettings.generated.h"


/**
 * Structure for auto-complete commands and their descriptions.
 */
USTRUCT()
struct FAutoCompleteCommand
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config, EditAnywhere, Category=Command)
	FString Command;

	UPROPERTY(config, EditAnywhere, Category=Command)
	FString Desc;

	bool operator<(const FAutoCompleteCommand& rhs) const
	{
		// sort them in opposite order for game console UI rendering (bottom up)
		return Command >= rhs.Command;
	}
};


/**
 * Implements the settings for the UConsole class.
 */
UCLASS(config=Input, defaultconfig)
class ENGINESETTINGS_API UConsoleSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/**  Visible Console stuff */
	UPROPERTY(globalconfig, EditAnywhere, Category=General)
	int32 MaxScrollbackSize;

	/** Manual list of auto-complete commands and info specified in BaseInput.ini */
	UPROPERTY(config, EditAnywhere, Category=AutoComplete)
	TArray<struct FAutoCompleteCommand> ManualAutoCompleteList;

	/** List of relative paths (e.g. Content/Maps) to search for map names for auto-complete usage. Specified in BaseInput.ini. */
	UPROPERTY(config, EditAnywhere, Category=AutoComplete)
	TArray<FString> AutoCompleteMapPaths;
};
