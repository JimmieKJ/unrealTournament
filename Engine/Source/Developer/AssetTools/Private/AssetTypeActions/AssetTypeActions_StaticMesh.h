// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/StaticMesh.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_StaticMesh : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_StaticMesh", "Static Mesh"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 255, 255); }
	virtual UClass* GetSupportedClass() const override { return UStaticMesh::StaticClass(); }
	virtual bool HasActions( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Basic; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	// End IAssetTypeActions

private:
	/** Handler for when CreateDestructibleMesh is selected */
	void ExecuteCreateDestructibleMesh(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	// Handler for when SaveGeneratedLODsInPackage is selected
	void ExecuteSaveGeneratedLODsInPackage(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handler to provide the list of LODs that can be imported or reimported */
	void GetImportLODMenu(class FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<UStaticMesh>> Objects);
};