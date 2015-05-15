// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UnrealTournament.h"
#include "UTGameRuleset.generated.h"

UCLASS(Config=Rules, perObjectConfig)
class UUTGameRuleset : public UObject
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

	// Holds a list of maps that can be played in this ruleset.
	UPROPERTY(Config)
	TArray<FString> MapPlaylist;

	// Holds the max # of maps in the rotation
	UPROPERTY(Config)
	int32 MapPlaylistSize;

	// The number of players needed to start.
	UPROPERTY(Config)
	int32 MinPlayersToStart;

	// The number of players allowed in this match.  NOTE: it must be duplicated in the GameOptions string.
	UPROPERTY(Config)
	int32 MaxPlayers;

	// Holds a string reference to the material to display that represents this rule
	UPROPERTY(Config)
	FString DisplayTexture;

	// Not displayed, this wholes the game type that will be passed to the server via the URL.  
	UPROPERTY(Config)
	FString GameMode;
	
	// Hold the ?xxxx options that will be used to start the server.  NOTE: this set of options will be parsed for display.
	UPROPERTY(Config)
	FString GameOptions;
	
	UPROPERTY(Config)
	TArray<FPackageRedirectReference> RedirectReferences;
};



