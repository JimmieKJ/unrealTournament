// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MediaSubtitles.generated.h"


USTRUCT(BlueprintType)
struct FMediaSubtitle
{
	GENERATED_BODY()

	FTimespan StartTime;
	FTimespan EndTime;
	FText Text;
	FIntPoint Position;
};


/**
 * Implements an asset for subtitles that can be attached to media sources.
 */
UCLASS(BlueprintType, hidecategories=(Object))
class MEDIAASSETS_API UMediaSubtitles
	: public UObject
{
	GENERATED_BODY()

private:

	/** The subtitle entries. */
	UPROPERTY()
	TArray<FMediaSubtitle> Subtitles;
};
