// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/MorphTarget.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_MorphTarget : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MorphTarget", "Morph Target"); }
	virtual FColor GetTypeColor() const override { return FColor(87,0,174); }
	virtual UClass* GetSupportedClass() const override { return UMorphTarget::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }

private:
	/** Handler for when Move to Mesh is requested **/
	void ExecuteMovetoMesh(TArray<TWeakObjectPtr<UMorphTarget>> Objects);
};