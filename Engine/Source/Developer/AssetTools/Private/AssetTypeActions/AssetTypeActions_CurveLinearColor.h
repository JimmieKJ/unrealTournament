// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Curves/CurveLinearColor.h"
#include "AssetTypeActions_Curve.h"

class FAssetTypeActions_CurveLinearColor : public FAssetTypeActions_Curve
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CurveLinearColor", "Color Curve"); }
	virtual UClass* GetSupportedClass() const override { return UCurveLinearColor::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};