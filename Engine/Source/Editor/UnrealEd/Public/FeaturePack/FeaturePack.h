// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once


class FFeaturePack
{
public:
	FFeaturePack();

	/* Import any pending feature packs. */
	void ImportPendingPacks();

	/* Parse and insert any feature packs flagged to include and import them */
	void ParsePacks();

private:
	struct PackData
	{
		FString PackSource;
		FString PackName;
		FString PackMap;
		TArray<UObject*>	ImportedObjects;
	};
	/* List of packs we imported */
	TArray<PackData>	ImportedPacks;
};
