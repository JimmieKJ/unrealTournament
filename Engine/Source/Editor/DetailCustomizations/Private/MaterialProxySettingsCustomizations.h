// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FMaterialProxySettingsCustomizations : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization instance */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

protected:
	EVisibility AreManualOverrideTextureSizesEnabled() const;
	EVisibility IsTextureSizeEnabled() const;

	EVisibility IsSimplygonMaterialMergingVisible() const;
	
	TSharedPtr< IPropertyHandle > EnumHandle;

	TSharedPtr< IPropertyHandle > TextureSizeHandle;
	TSharedPtr< IPropertyHandle > DiffuseTextureSizeHandle;
	TSharedPtr< IPropertyHandle > NormalTextureSizeHandle;
	TSharedPtr< IPropertyHandle > MetallicTextureSizeHandle;
	TSharedPtr< IPropertyHandle > RoughnessTextureSizeHandle;
	TSharedPtr< IPropertyHandle > SpecularTextureSizeHandle;
	TSharedPtr< IPropertyHandle > EmissiveTextureSizeHandle;
	TSharedPtr< IPropertyHandle > OpacityTextureSizeHandle;

	TSharedPtr< IPropertyHandle > MergeTypeHandle;
	TSharedPtr< IPropertyHandle > GutterSpaceHandle;
};
