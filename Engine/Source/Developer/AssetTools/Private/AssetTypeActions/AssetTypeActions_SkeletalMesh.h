// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetTypeActions_SkeletalMesh : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SkeletalMesh", "Skeletal Mesh"); }
	virtual FColor GetTypeColor() const override { return FColor(255,0,255); }
	virtual UClass* GetSupportedClass() const override { return USkeletalMesh::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Basic | EAssetTypeCategories::Animation; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;

protected:
	/** Gets additional actions that do not apply to destructible meshes */
	virtual void GetNonDestructibleActions( const TArray<TWeakObjectPtr<USkeletalMesh>>& Meshes, FMenuBuilder& MenuBuilder);

private:
	/** Handler for when skeletal mesh LOD import is selected */
	void LODImport(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for when skeletal mesh LOD sub menu is opened */
	void GetLODMenu(class FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for when NewPhysicsAsset is selected */
	void ExecuteNewPhysicsAsset(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);
	
	/** Handler for when NewSkeleton is selected */
	void ExecuteNewSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for when AssignSkeleton is selected */
	void ExecuteAssignSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for when FindSkeleton is selected */
	void ExecuteFindSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	// Helper functions
private:
	/** Creates a physics asset based on the mesh */
	void CreatePhysicsAssetFromMesh(USkeletalMesh* SkelMesh) const;

	/** Assigns a skeleton to the mesh */
	void AssignSkeletonToMesh(USkeletalMesh* SkelMesh) const;

	void OnAssetCreated(TArray<UObject*> NewAssets) const;

	void FillSourceMenu(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const;
	void FillSkeletonMenu(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const;
	void FillCreateMenu(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const;
};
