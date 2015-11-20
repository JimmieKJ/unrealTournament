// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FNavAgentSelectorCustomization : public IPropertyTypeCustomization
{
public:

	// IPropertyTypeCustomization interface

	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

public:

	/**
	 * Creates a new instance.
	 *
	 * @return A new struct customization for Keys.
	 */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	void OnAgentStateChanged();
	FText GetSupportedDesc() const;

	TSharedPtr<IPropertyHandle> StructHandle;
	FText SupportedDesc;
};
