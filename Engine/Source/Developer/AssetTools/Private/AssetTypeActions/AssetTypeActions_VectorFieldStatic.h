// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AssetTypeActions/AssetTypeActions_VectorField.h"
#include "VectorField/VectorFieldStatic.h"

class FAssetTypeActions_VectorFieldStatic : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_VectorFieldStatic", "Static Vector Field"); }
	virtual FColor GetTypeColor() const override { return FColor(200, 128, 128); }
	virtual UClass* GetSupportedClass() const override { return UVectorFieldStatic::StaticClass(); }
	virtual bool CanFilter() override { return true; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;

};