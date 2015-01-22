// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/TextureRenderTargetCube.h"
#include "AssetTypeActions_TextureRenderTarget.h"

class FAssetTypeActions_TextureRenderTargetCube : public FAssetTypeActions_TextureRenderTarget
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TextureRenderTargetCube", "Cube Render Target"); }
	virtual UClass* GetSupportedClass() const override { return UTextureRenderTargetCube::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};