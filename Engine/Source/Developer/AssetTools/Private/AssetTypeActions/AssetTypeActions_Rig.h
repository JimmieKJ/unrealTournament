// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/Rig.h"

class FAssetTypeActions_Rig : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Rig", "Rig"); }
	virtual FColor GetTypeColor() const override { return FColor(255,201,14); }
	virtual UClass* GetSupportedClass() const override { return URig::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
};