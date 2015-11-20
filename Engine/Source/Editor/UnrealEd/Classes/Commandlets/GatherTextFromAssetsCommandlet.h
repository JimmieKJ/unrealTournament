// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Commandlets/GatherTextCommandletBase.h"
#include "Sound/DialogueTypes.h"
#include "GatherTextFromAssetsCommandlet.generated.h"

namespace EAssetTextGatherStatus
{
	enum 
	{
		BadBitMask = 1
	};
	enum Type
	{
		//							ErrorType << 1 + BadBit
		None						= (0 << 1) + 0,
		MissingKey					= (1 << 1) + 1,
		MissingKey_Resolved			= (1 << 1) + 0,
		IdentityConflict			= (2 << 1) + 1,
		IdentityConflict_Resolved	= (2 << 1) + 0,
	};
}

class FLocMetadataValue;

/**
 *	UGatherTextFromAssetsCommandlet: Localization commandlet that collects all text to be localized from the game assets.
 */
UCLASS()
class UGatherTextFromAssetsCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()

	void ProcessGatherableTextDataArray(const FString& PackageFilePath, const TArray<FGatherableTextData>& GatherableTextDataArray);
	void ProcessPackages( const TArray< UPackage* >& PackagesToProcess );

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

private:
	struct FConflictTracker
	{
		struct FEntry
		{
			FString PathToSourceString;
			TSharedPtr<FString, ESPMode::ThreadSafe> SourceString;
			EAssetTextGatherStatus::Type Status;
		};

		typedef TArray<FEntry> FEntryArray;
		typedef TMap<FString, FEntryArray> FKeyTable;
		typedef TMap<FString, FKeyTable> FNamespaceTable;

		FNamespaceTable Namespaces;
	};

private:
	static const FString UsageText;

	FConflictTracker ConflictTracker;

	bool bFixBroken;
	bool ShouldGatherFromEditorOnlyData;
};
