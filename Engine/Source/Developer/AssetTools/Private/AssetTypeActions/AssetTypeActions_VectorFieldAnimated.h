// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AssetTypeActions_VectorField.h"
#include "VectorField/VectorFieldAnimated.h"

class FAssetTypeActions_VectorFieldAnimated : public FAssetTypeActions_VectorField
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_VectorFieldAnimated", "Animated Vector Field"); }
	virtual UClass* GetSupportedClass() const override { return UVectorFieldAnimated::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};