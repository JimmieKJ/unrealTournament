// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetTypeActions_SubDSurface : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SubDSurface", "Subdivision Surface"); }
	virtual FColor GetTypeColor() const override { return FColor(128, 196, 128); }
	virtual UClass* GetSupportedClass() const override;

	virtual uint32 GetCategories() override { return EAssetTypeCategories::MaterialsAndTextures; }
};