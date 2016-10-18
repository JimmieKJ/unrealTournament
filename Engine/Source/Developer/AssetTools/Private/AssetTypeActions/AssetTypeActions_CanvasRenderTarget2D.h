// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/CanvasRenderTarget2D.h"
#include "AssetTypeActions_TextureRenderTarget2D.h"

class FAssetTypeActions_CanvasRenderTarget2D : public FAssetTypeActions_TextureRenderTarget2D
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CanvasRenderTarget2D", "Canvas Render Target"); }
	virtual UClass* GetSupportedClass() const override { return UCanvasRenderTarget2D::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};