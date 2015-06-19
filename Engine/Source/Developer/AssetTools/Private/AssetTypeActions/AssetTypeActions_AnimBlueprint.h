// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetTypeActions_AnimBlueprint : public FAssetTypeActions_Blueprint
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimBlueprint", "Animation Blueprint"); }
	virtual FColor GetTypeColor() const override { return FColor(200,116,0); }
	virtual UClass* GetSupportedClass() const override { return UAnimBlueprint::StaticClass(); }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;

	// FAssetTypeActions_Blueprint interface
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:

	/** Handler to fill the retarget submenu */
	void FillRetargetMenu( FMenuBuilder& MenuBuilder, const TArray<UObject*> InObjects );

	/** Handler for when FindSkeleton is selected */
	void ExecuteFindSkeleton(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects);

	/** Context menu item handler for changing the supplied assets skeletons */ 
	void RetargetAssets(TArray<UObject*> InAnimBlueprints, bool bDuplicateAssets);

	/** Handler for retargeting */
	void RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bConvertSpaces, bool bDuplicateAssets, TArray<TWeakObjectPtr<UObject>> AnimBlueprints);
};