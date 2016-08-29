// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AlembicLibraryPublicPCH.h"
#include "AbcImportSettingsCustomization.h"

#include "AbcImportSettings.h"

#include "DetailLayoutBuilder.h"

void FAbcImportSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder)
{
	TSharedRef<IPropertyHandle> ImportType = LayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAbcImportSettings, ImportType));
	
	uint8 EnumValue;
	ImportType->GetValue(EnumValue);
	IDetailCategoryBuilder& CompressionBuilder = LayoutBuilder.EditCategory("Compression");
	CompressionBuilder.SetCategoryVisibility(EnumValue == (uint8)EAlembicImportType::Skeletal);

	IDetailCategoryBuilder& StaticMeshBuilder = LayoutBuilder.EditCategory("StaticMesh");
	StaticMeshBuilder.SetCategoryVisibility(EnumValue == (uint8)EAlembicImportType::StaticMesh);

	FSimpleDelegate OnImportTypeChangedDelegate = FSimpleDelegate::CreateSP(this, &FAbcImportSettingsCustomization::OnImportTypeChanged, &LayoutBuilder);
	ImportType->SetOnPropertyValueChanged(OnImportTypeChangedDelegate);
}

TSharedRef<IDetailCustomization> FAbcImportSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAbcImportSettingsCustomization());
}

void FAbcImportSettingsCustomization::OnImportTypeChanged(IDetailLayoutBuilder* LayoutBuilder)
{
	LayoutBuilder->ForceRefreshDetails();
}

TSharedRef<IPropertyTypeCustomization> FAbcSamplingSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAbcSamplingSettingsCustomization);
}

void FAbcSamplingSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	Settings = UAbcImportSettings::Get();

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);
	
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		IDetailPropertyRow& Property = StructBuilder.AddChildProperty(ChildHandle);
		static const FName EditConditionName = "EnumCondition";
		int32 EnumCondition = ChildHandle->GetINTMetaData(EditConditionName);
		Property.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FAbcSamplingSettingsCustomization::ArePropertiesVisible, EnumCondition)));
	}
}

EVisibility FAbcSamplingSettingsCustomization::ArePropertiesVisible(const int32 VisibleType) const
{
	return (Settings->SamplingSettings.SamplingType == (EAlembicSamplingType)VisibleType || VisibleType == 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<IPropertyTypeCustomization> FAbcCompressionSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAbcCompressionSettingsCustomization);
}

void FAbcCompressionSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	Settings = UAbcImportSettings::Get();

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		IDetailPropertyRow& Property = StructBuilder.AddChildProperty(ChildHandle);
		static const FName EditConditionName = "EnumCondition";
		int32 EnumCondition = ChildHandle->GetINTMetaData(EditConditionName);
		Property.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FAbcCompressionSettingsCustomization::ArePropertiesVisible, EnumCondition)));
	}
}

EVisibility FAbcCompressionSettingsCustomization::ArePropertiesVisible(const int32 VisibleType) const
{
	return (Settings->CompressionSettings.BaseCalculationType == (EBaseCalculationType)VisibleType || VisibleType == 0) ? EVisibility::Visible : EVisibility::Collapsed;
}
