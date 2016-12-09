// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Curve.h"
#include "Curves/CurveLinearColor.h"

class FAssetTypeActions_CurveLinearColor : public FAssetTypeActions_Curve
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CurveLinearColor", "Color Curve"); }
	virtual UClass* GetSupportedClass() const override { return UCurveLinearColor::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
