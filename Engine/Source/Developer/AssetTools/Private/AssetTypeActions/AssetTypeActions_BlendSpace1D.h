// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/BlendSpace1D.h"
#include "AssetTypeActions_AnimationAsset.h"

class FAssetTypeActions_BlendSpace1D : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_BlendSpace1D", "Blend Space 1D"); }
	virtual FColor GetTypeColor() const override { return FColor(255,180,130); }
	virtual UClass* GetSupportedClass() const override { return UBlendSpace1D::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};