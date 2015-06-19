// editor-creatable bot character data
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBotCharacter.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTBotCharacter : public UDataAsset
{
	GENERATED_BODY()
public:
	/** alternate names to use when there are not enough bot characters for the number of players requested and we need to put in duplicates */
	UPROPERTY(EditAnywhere)
	TArray<FString> AltNames;

	/** bot skill rating (0 - 7.9) */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Meta = (ClampMin = "0", UIMin = "0", ClampMax = "7.9", UIMax = "7.9"))
	float Skill;
	/** bot personality attributes affecting behavior */
	UPROPERTY(EditAnywhere)
	FBotPersonality Personality;
	
	/** character content (primary mesh/material) */
	UPROPERTY(EditAnywhere, Meta = (MetaClass = "UTCharacterContent"))
	FStringClassReference Character;

	/** hat to use */
	UPROPERTY(EditAnywhere, Meta = (MetaClass = "UTHat"))
	FStringClassReference HatType;
	/** eyewear to use */
	UPROPERTY(EditAnywhere, Meta = (MetaClass = "UTEyewear"))
	FStringClassReference EyewearType;
};