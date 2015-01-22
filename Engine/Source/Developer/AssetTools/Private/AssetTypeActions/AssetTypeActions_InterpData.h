// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Matinee/InterpData.h"

class FAssetTypeActions_InterpData : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_InterpData", "Matinee Data"); }
	virtual FColor GetTypeColor() const override { return FColor(255,128,0); }
	virtual UClass* GetSupportedClass() const override { return UInterpData::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
};