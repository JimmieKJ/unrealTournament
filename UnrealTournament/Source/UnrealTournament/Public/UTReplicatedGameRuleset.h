// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTGameRuleset.h"
#include "UTReplicatedGameRuleset.generated.h"

UCLASS()
class AUTReplicatedGameRuleset : public AInfo
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(Replicated, ReplicatedUsing = GotTag, BlueprintReadOnly, Category = Ruleset)
	FString UniqueTag;

	// Holds a list of Ruleset Categories this rule set should show up in.  This determines which tabs this rule is sorted in to
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	TArray<FName> Categories;

	// Holds the of this rule set.  It will be displayed over the badge
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FString Title;

	// Holds the description for this Ruleset.  It will be displayed above the rules selection window.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FString Tooltip;

	// Holds the description for this Ruleset.  It will be displayed above the rules selection window.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FString Description;

	// Holds the max # of maps in the rotation
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	int32 MapPlaylistSize;

	// Holds a list of maps that can be played in this ruleset.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	TArray<FString> MapPlaylist;

	// The number of players needed to start.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	int32 MinPlayersToStart;

	// The number of players allowed in this match.  NOTE: it must be duplicated in the GameOptions string.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	int32 MaxPlayers;

	// Holds a string reference to the material to show for this rule's badge
	UPROPERTY(Replicated, ReplicatedUsing = BuildSlateBadge, BlueprintReadOnly, Category = Ruleset)
	FString DisplayTexture;

	// Why have GameMode and GameModeClass?  GameMode is passed on the URL/Command line and should be as short as possible.
	// You can set up Aliases in DefaultGame.  But we need GameModeClass at times to grab the default object.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	FString GameModeClass;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Ruleset)
	uint32 bCustomRuleset:1;

	UPROPERTY(Replicated)
	TArray<FPackageRedirectReference> RequiredPackages;

	// -------------- These are server side only.

	// Not displayed, this wholes the game type that will be passed to the server via the URL.  
	UPROPERTY(BlueprintReadOnly, Category = Ruleset)
	FString GameMode;
	
	// Hold the ?xxxx options that will be used to start the server.  NOTE: this set of options will be parsed for display.
	UPROPERTY(BlueprintReadOnly, Category = Ruleset)
	FString GameOptions;

	UFUNCTION()
	void SetRules(UUTGameRuleset* NewRules);

#if !UE_SERVER
	TSharedPtr<FSlateDynamicImageBrush> SlateBadge;
	const FSlateBrush* GetSlateBadge() const;
#endif

	// When the material reference arrives, build the slate texture
	UFUNCTION()
	virtual void BuildSlateBadge();

	UFUNCTION()
	virtual AUTGameMode* GetDefaultGameModeObject();

protected:

	UFUNCTION()
	virtual void GotTag();


	FString Fixup(FString OldText);

};



