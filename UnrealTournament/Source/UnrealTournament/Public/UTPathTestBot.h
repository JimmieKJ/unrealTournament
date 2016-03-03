// Bot subclass used for testing special move paths in levels; teleports to and attempts them in sequence
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBot.h"

#include "UTPathTestBot.generated.h"

USTRUCT()
struct FPathTestEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FRouteCacheItem Src;
	UPROPERTY()
	FRouteCacheItem Dest;

	FPathTestEntry()
	{}
	FPathTestEntry(const FRouteCacheItem& InSrc, const FRouteCacheItem& InDest)
		: Src(InSrc), Dest(InDest)
	{}
};

UCLASS()
class AUTPathTestBot : public AUTBot
{
	GENERATED_BODY()
protected:
	virtual void ExecuteWhatToDoNext() override;

	UPROPERTY()
	TArray<FPathTestEntry> TestList;
	UPROPERTY()
	int32 CurrentTestIndex;

	FVector LastPawnLoc;

public:
	virtual void GatherTestList(bool bHighJumps, bool bWallDodges, bool bLifts, bool bLiftJumps);
};