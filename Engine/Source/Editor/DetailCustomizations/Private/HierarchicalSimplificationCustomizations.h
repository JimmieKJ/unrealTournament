// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FHierarchicalSimplificationCustomizations : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization instance */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

protected:
	EVisibility IsSimplifyMeshVisible() const;
	EVisibility IsProxyMeshSettingVisible() const;
	EVisibility IsMergeMeshSettingVisible() const;

	TSharedPtr< IPropertyHandle > SimplifyMeshPropertyHandle;
};
