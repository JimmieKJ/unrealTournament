// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AlembicLibraryPublicPCH.h"

#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"

class UAbcImportSettings;

class FAbcImportSettingsCustomization : public IDetailCustomization
{
public:
	/** Begin IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder) override;
	/** End IDetailCustomization interface */
public:

	/** Creates a new instance. */
	static TSharedRef<IDetailCustomization> MakeInstance();
protected:
	void OnImportTypeChanged(IDetailLayoutBuilder* LayoutBuilder);

protected:
	UAbcImportSettings* ImportSettings;
};

/** AbcSampling settings customizations handles hiding/showing properties according to the frame sampling type */
class FAbcSamplingSettingsCustomization
	: public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	FAbcSamplingSettingsCustomization()
	{ }

	virtual ~FAbcSamplingSettingsCustomization() {}

	/** Begin IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	/** End IPropertyTypeCustomization interface */

protected:
	EVisibility ArePropertiesVisible(const int32 VisibleType) const;
	UAbcImportSettings* Settings;
};

/** AbcCompression settings customizations handles hiding/showing properties according to the base calculation type */
class FAbcCompressionSettingsCustomization
	: public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	FAbcCompressionSettingsCustomization()
	{ }

	virtual ~FAbcCompressionSettingsCustomization() {}
	
	/** Begin IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	/** End IPropertyTypeCustomization interface */

protected:
	EVisibility ArePropertiesVisible(const int32 VisibleType) const;
	UAbcImportSettings* Settings;
};