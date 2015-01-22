// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/VertexAnim/VertexAnimation.h"

class FAssetTypeActions_VertexAnimation : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_VertexAnimation", "Vertex Animation"); }
	virtual FColor GetTypeColor() const override { return FColor(87,0,174); }
	virtual UClass* GetSupportedClass() const override { return UVertexAnimation::StaticClass(); }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
	
	// Simply returns false. Vertex Animations are currently are unsupported. Rather than remove
	// the files in question; returning false from here disables the filer in the content browser.
	// Remove this when vertex anims are supported again.
	virtual bool CanFilter() override;
};