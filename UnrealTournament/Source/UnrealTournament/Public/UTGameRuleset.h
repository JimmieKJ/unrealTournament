// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UnrealTournament.h"
#include "UTGameRuleset.generated.h"

UCLASS(Config=Rules, perObjectConfig)
class UNREALTOURNAMENT_API UUTGameRuleset : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// Holds the name of this rule set.  NOTE it must be unique on this server.  
	UPROPERTY(Config)
	FString UniqueTag;

	// Holds a list of Ruleset Categories this rule set should show up in.  This determines which tabs this rule is sorted in to
	UPROPERTY(Config)
	TArray<FName> Categories;

	// Holds the of this rule set.  It will be displayed over the badge
	UPROPERTY(Config)
	FString Title;

	// Holds the description for this Ruleset.  It will be displayed above the rules selection window.
	UPROPERTY(Config)
	FString Tooltip;

	// Holds the description for this Ruleset.  It will be displayed above the rules selection window.
	UPROPERTY(Config)
	FString Description;

	UPROPERTY(Config)
	TArray<FString> MapPrefixes;

	// Holds the Max # of maps available in this maplist or 0 for no maximum
	UPROPERTY(Config)
	int32 MaxMapsInList;
	
	// Which epic maps to include.  This can't be adjusted via the ini and will be added
	// to the map list before the custom maps.
	UPROPERTY()
	FString EpicMaps;	

	// The default map to use
	UPROPERTY(Config)
	FString DefaultMap;

	UPROPERTY(Config)
	TArray<FConfigMapInfo> CustomMapList;

	// The number of players needed to start.
	UPROPERTY(Config)
	int32 MinPlayersToStart;

	// The number of players allowed in this match.  NOTE: it must be duplicated in the GameOptions string.
	UPROPERTY(Config)
	int32 MaxPlayers;

	// Holds a string reference to the material to display that represents this rule
	UPROPERTY(Config)
	FString DisplayTexture;

	// Not displayed, this holds the game type that will be passed to the server via the URL.  
	UPROPERTY(Config)
	FString GameMode;
	
	// Hold the ?xxxx options that will be used to start the server.  NOTE: this set of options will be parsed for display.
	UPROPERTY(Config)
	FString GameOptions;
	
	UPROPERTY(Config)
	TArray<FString> RequiredPackages;

	UPROPERTY(Config)
	uint32 bTeamGame:1;
};



