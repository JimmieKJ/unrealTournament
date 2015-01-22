// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSkinnedMeshComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	void CreateActuallyUsedPhysicsAssetWidget(FDetailWidgetRow& OutWidgetRow, IDetailLayoutBuilder* DetailBuilder) const;

	FText GetUsedPhysicsAssetAsText(IDetailLayoutBuilder* DetailBuilder) const;
	void BrowseUsedPhysicsAsset(IDetailLayoutBuilder* DetailBuilder) const;

	bool FindUniqueUsedPhysicsAsset(IDetailLayoutBuilder* DetailBuilder, UPhysicsAsset*& OutFoundPhysicsAsset) const;
};
