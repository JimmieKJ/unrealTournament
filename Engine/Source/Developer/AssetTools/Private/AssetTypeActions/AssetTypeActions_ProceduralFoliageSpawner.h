// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetTypeActions_ProceduralFoliageSpawner : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ProceduralFoliageSpawner", "Procedural Foliage Spawner"); }
	virtual FColor GetTypeColor() const override { return FColor(7, 103, 7); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool CanFilter() override;
};