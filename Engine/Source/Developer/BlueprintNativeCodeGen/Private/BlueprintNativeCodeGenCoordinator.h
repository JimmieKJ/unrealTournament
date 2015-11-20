// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetData.h"
#include "BlueprintNativeCodeGenManifest.h"
#include "BlueprintNativeCodeGenCoordinator.generated.h"

// Forward declarations
struct FNativeCodeGenCommandlineParams;
struct FConvertedAssetRecord;

/**  */
UCLASS(config=Editor)
class UBlueprintNativeCodeGenConfig : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(globalconfig)
	TArray<FString> PackagesToAlwaysConvert;
};

/**  */
struct FBlueprintNativeCodeGenCoordinator
{
public:
	FBlueprintNativeCodeGenCoordinator(const FNativeCodeGenCommandlineParams& CommandlineParams);

	const FBlueprintNativeCodeGenManifest& GetManifest() { return Manifest; }

	bool IsTargetedForConversion(const FString AssetPath);
	bool IsTargetedForConversion(const UBlueprint* Blueprint);
	bool IsTargetedForConversion(const UClass* Class);
	bool IsTargetedForConversion(const UEnum* Enum);
	bool IsTargetedForConversion(const UStruct* Struct);

	DECLARE_DELEGATE_RetVal_OneParam(bool, FConversionDelegate, FConvertedAssetRecord&);
	bool ProcessConversionQueue(const FConversionDelegate& ConversionDelegate);

private:
	/**  */
	TArray<FAssetData> ConversionQueue;
	/**  */
	FBlueprintNativeCodeGenManifest Manifest;
};

