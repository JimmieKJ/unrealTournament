// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AssetImportData.generated.h"

/**
 * A class to hold import options and data for assets. This class has many children in editor code.
 */
UCLASS(MinimalAPI)
class UAssetImportData : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Path to the resource used to construct this static mesh. Relative to the object's package, BaseDir() or absolute */
	UPROPERTY(EditAnywhere, Category=ImportSettings)
	FString SourceFilePath;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY(VisibleAnywhere, Category=ImportSettings)
	FString SourceFileTimestamp;

	/** If true, the settings have been modified but not reimported yet */
	UPROPERTY()
	uint32 bDirty : 1;

	// UObject interface
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End of UObject interface
};



