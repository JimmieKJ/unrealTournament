// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FStaticMeshComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	/**
	 * @return Whether or not override lightmap res is enabled
	 */
	bool IsOverrideLightmapResEnabled() const;
private:
	TSharedPtr<IPropertyHandle> EnableOverrideLightmapRes;
};
