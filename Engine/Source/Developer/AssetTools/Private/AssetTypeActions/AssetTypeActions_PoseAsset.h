// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/PoseAsset.h"
#include "AssetTypeActions_AnimationAsset.h"

class FAssetTypeActions_PoseAsset : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PoseAsset", "Pose Asset"); }
	virtual FColor GetTypeColor() const override { return FColor(237, 28, 36); }
	virtual UClass* GetSupportedClass() const override { return UPoseAsset::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};