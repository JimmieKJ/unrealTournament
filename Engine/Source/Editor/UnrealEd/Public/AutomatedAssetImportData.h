// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AutomatedAssetImportData.generated.h"

class UFactory;

DECLARE_LOG_CATEGORY_EXTERN(LogAutomatedImport, Log, All);

/**
 * Contains data for a group of assets to import
 */ 
UCLASS(Transient)
class UNREALED_API UAutomatedAssetImportData : public UObject
{
	GENERATED_BODY()

public:
	UAutomatedAssetImportData();

	/** @return true if this group contains enough valid data to import*/
	bool IsValid() const;

	/** Initalizes the group */
	void Initialize();

	/** @return the display name of the group */
	FString GetDisplayName() const; 
public:
	/** Display name of the group. This is for logging purposes only. */
	UPROPERTY()
	FString GroupName;

	/** Filenames to import */
	UPROPERTY()
	TArray<FString> Filenames;

	/** Content path in the projects content directory where assets will be imported */
	UPROPERTY()
	FString DestinationPath;

	/** Name of the factory to use when importing these assets. If not specified the factory type will be auto detected */
	UPROPERTY()
	FString FactoryName;

	/** Whether or not to replace existing assets */
	UPROPERTY()
	bool bReplaceExisting;

	/** Whether or not to skip importing over read only assets that could not be checked out */
	UPROPERTY()
	bool bSkipReadOnly;

	/** Pointer to the factory currently being sued */
	UPROPERTY()
	UFactory* Factory;

};
