// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

/**
 * Customizes a Distance Datum struct to improve naming when used as a parameter
 */
class FDistanceDatumStructCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/**
	 * Destructor
	 */
	virtual ~FDistanceDatumStructCustomization();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

private:
	/**
	 * Constructor
	 */
	FDistanceDatumStructCustomization();
};
