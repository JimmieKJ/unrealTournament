// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetTypeActions_InstancedFoliageSettings : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_InstancedFoliageSettings", "Foliage Type"); }
	virtual FColor GetTypeColor() const override { return FColor(12, 173, 12); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};