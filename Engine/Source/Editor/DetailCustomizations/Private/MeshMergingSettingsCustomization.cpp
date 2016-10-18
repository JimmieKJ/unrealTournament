// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "MeshMergingSettingsCustomization.h"
#include "Engine/MeshMerging.h"
#include "PropertyRestriction.h"

#define LOCTEXT_NAMESPACE "FMeshMergingSettingCustomization"

FMeshMergingSettingsObjectCustomization::~FMeshMergingSettingsObjectCustomization()
{
}

void FMeshMergingSettingsObjectCustomization::CustomizeDetails(IDetailLayoutBuilder& LayoutBuilder)
{
	TSharedRef<IPropertyHandle> SettingsHandle = LayoutBuilder.GetProperty(FName("UMeshMergingSettingsObject.Settings"));
	
	FName MeshCategory("MeshSettings");
	IDetailCategoryBuilder& MeshCategoryBuilder = LayoutBuilder.EditCategory(MeshCategory);

	TArray<TSharedRef<IPropertyHandle>> SimpleDefaultProperties;
	MeshCategoryBuilder.GetDefaultProperties(SimpleDefaultProperties, true, true);
	MeshCategoryBuilder.AddProperty(SettingsHandle);

	FName CategoryMetaData("Category");
	for (TSharedRef<IPropertyHandle> Property: SimpleDefaultProperties)
	{
		const FString CategoryName = Property->GetMetaData(CategoryMetaData);

		IDetailCategoryBuilder& CategoryBuilder = LayoutBuilder.EditCategory(*CategoryName);
		IDetailPropertyRow& PropertyRow = CategoryBuilder.AddProperty(Property);

		if (Property->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FMeshMergingSettings, SpecificLOD))
		{
			static const FName EditConditionName = "EnumCondition";
			int32 EnumCondition = Property->GetINTMetaData(EditConditionName);
			PropertyRow.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FMeshMergingSettingsObjectCustomization::ArePropertiesVisible, EnumCondition)));
		}
		else if (Property->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FMeshMergingSettings, LODSelectionType))
		{
			EnumProperty = Property;
						
			FText RestrictReason = FText::FromString("Unable to support this option in Merge Actor");
			TSharedPtr<FPropertyRestriction> EnumRestriction = MakeShareable(new FPropertyRestriction(RestrictReason));
			EnumRestriction->AddValue(TEXT("Calculate correct LOD level"));
			EnumProperty->AddRestriction(EnumRestriction.ToSharedRef());
		}
	}

	FName MaterialCategory("MaterialSettings");
	IDetailCategoryBuilder& MaterialCategoryBuilder = LayoutBuilder.EditCategory(MaterialCategory);
	SimpleDefaultProperties.Empty();
	MaterialCategoryBuilder.GetDefaultProperties(SimpleDefaultProperties, true, true);

	for (TSharedRef<IPropertyHandle> Property : SimpleDefaultProperties)
	{
		const FString CategoryName = Property->GetMetaData(CategoryMetaData);

		IDetailCategoryBuilder& CategoryBuilder = LayoutBuilder.EditCategory(*CategoryName);
		IDetailPropertyRow& PropertyRow = CategoryBuilder.AddProperty(Property);

		// Disable material settings if we are exporting all LODs (no support for material baking in this case)
		if (CategoryName.Compare("MaterialSettings") == 0)
		{
			PropertyRow.IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FMeshMergingSettingsObjectCustomization::AreMaterialPropertiesEnabled)));
		}
	}
}

TSharedRef<IDetailCustomization> FMeshMergingSettingsObjectCustomization::MakeInstance()
{
	return MakeShareable(new FMeshMergingSettingsObjectCustomization);
}

EVisibility FMeshMergingSettingsObjectCustomization::ArePropertiesVisible(const int32 VisibleType) const
{
	uint8 CurrentEnumValue = 0;
	EnumProperty->GetValue(CurrentEnumValue);
	return (CurrentEnumValue == VisibleType) ? EVisibility::Visible : EVisibility::Collapsed;
}

bool FMeshMergingSettingsObjectCustomization::AreMaterialPropertiesEnabled() const
{
	uint8 CurrentEnumValue = 0;
	EnumProperty->GetValue(CurrentEnumValue);

	return !(CurrentEnumValue == (uint8)EMeshLODSelectionType::AllLODs);
}

#undef LOCTEXT_NAMESPACE
