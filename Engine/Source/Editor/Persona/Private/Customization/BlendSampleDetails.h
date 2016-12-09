// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class FAssetData;

class FBlendSampleDetails : public IDetailCustomization
{
public:
	FBlendSampleDetails(const class UBlendSpaceBase* InBlendSpace, class SBlendSpaceGridWidget* InGridWidget);

	static TSharedRef<IDetailCustomization> MakeInstance(const class UBlendSpaceBase* InBlendSpace, class SBlendSpaceGridWidget* InGridWidget)
	{
		return MakeShareable( new FBlendSampleDetails(InBlendSpace, InGridWidget) );
	}

	// Begin IDetailCustomization interface
	virtual void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder) override;
	// End IDetailCustomization interface
protected:
	/** Checks whether or not the specified asset should not be shown in the mini content browser when changing the animation */
	bool ShouldFilterAsset(const FAssetData& AssetData) const;
private:
	/** Pointer to the current parent blend space for the customized blend sample*/
	const class UBlendSpaceBase* BlendSpace;
	/** Parent grid widget object */
	class SBlendSpaceGridWidget* GridWidget;
	/** Cached flags to check whether or not an additive animation type is compatible with the blend space*/	
	TMap<FString, bool> bValidAdditiveTypes;
};
