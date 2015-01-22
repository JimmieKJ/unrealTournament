// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/AimOffsetBlendSpace.h"
#include "AssetTypeActions_BlendSpace.h"

class FAssetTypeActions_AimOffset : public FAssetTypeActions_BlendSpace
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AimOffset", "Aim Offset"); }
	virtual FColor GetTypeColor() const override { return FColor(0,162,232); }
	virtual UClass* GetSupportedClass() const override { return UAimOffsetBlendSpace::StaticClass(); }
};