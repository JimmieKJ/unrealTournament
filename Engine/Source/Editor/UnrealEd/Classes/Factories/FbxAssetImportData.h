// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorFramework/AssetImportData.h"
#include "FbxAssetImportData.generated.h"

/**
 * Base class for import data and options used when importing any asset from FBX
 */
UCLASS(config=EditorPerProjectUserSettings, HideCategories=Object, abstract)
class UNREALED_API UFbxAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	FVector ImportTranslation;

	UPROPERTY(EditAnywhere, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	FRotator ImportRotation;

	UPROPERTY(EditAnywhere, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	float ImportUniformScale;
};