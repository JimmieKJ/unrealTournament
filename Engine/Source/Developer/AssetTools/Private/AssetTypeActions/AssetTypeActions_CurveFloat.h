// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Curves/CurveFloat.h"
#include "AssetTypeActions_Curve.h"

class FAssetTypeActions_CurveFloat : public FAssetTypeActions_Curve
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CurveFloat", "Float Curve"); }
	virtual UClass* GetSupportedClass() const override { return UCurveFloat::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};