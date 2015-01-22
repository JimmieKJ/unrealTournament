// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetTypeActions_TextureLightProfile : public FAssetTypeActions_Texture
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureLightProfile", "Texture Light Profile"); }
	virtual FColor GetTypeColor() const override { return FColor(192,64,192); }
	virtual UClass* GetSupportedClass() const override { return UTextureLightProfile::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};