// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/UnrealEd/Classes/Factories/FbxSceneImportData.h"

class FAssetTypeActions_SceneImportData : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SceneImportData", "Fbx Scene Import Data"); }
	virtual FColor GetTypeColor() const override { return FColor(255,0,255); }
	virtual UClass* GetSupportedClass() const override { return UFbxSceneImportData::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;

};
