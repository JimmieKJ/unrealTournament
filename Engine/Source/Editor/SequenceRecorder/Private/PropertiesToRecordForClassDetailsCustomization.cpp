// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "PropertiesToRecordForClassDetailsCustomization.h"
#include "SClassPropertyRecorderSettings.h"
#include "SequenceRecorderSettings.h"

void FPropertiesToRecordForClassDetailsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.
	NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	];
}

void FPropertiesToRecordForClassDetailsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedRef<IPropertyHandle> ClassProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPropertiesToRecordForClass, Class)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertiesProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPropertiesToRecordForClass, Properties)).ToSharedRef();

	ChildBuilder.AddChildProperty(ClassProperty);

	ChildBuilder.AddChildProperty(PropertiesProperty)
	.CustomWidget()
	.NameContent()
	[
		PropertiesProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(300.0f)
	.MaxDesiredWidth(400.0f)
	[
		SNew(SClassPropertyRecorderSettings, ClassProperty, PropertiesProperty, CustomizationUtils)
	];
}