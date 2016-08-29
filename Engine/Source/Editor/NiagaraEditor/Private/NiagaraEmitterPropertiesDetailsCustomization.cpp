// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraEditorModule.h"
#include "EditorSupportDelegates.h"
#include "UnrealEd.h"
#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"
#include "NiagaraEffectEditor.h"
#include "NiagaraEmitterPropertiesDetailsCustomization.h"
#include "ScopedTransaction.h"
#include "DetailWidgetRow.h"
#include "AssetData.h"

#define LOCTEXT_NAMESPACE "NiagaraEffectEditor"

TSharedRef<IDetailCustomization> FNiagaraEmitterPropertiesDetails::MakeInstance(TWeakObjectPtr<UNiagaraEmitterProperties> EmitterProperties)
{
	return MakeShareable(new FNiagaraEmitterPropertiesDetails(EmitterProperties));
}

FNiagaraEmitterPropertiesDetails::FNiagaraEmitterPropertiesDetails(TWeakObjectPtr<UNiagaraEmitterProperties> EmitterProperties)
: EmitterProps(EmitterProperties)
{
}

void FNiagaraEmitterPropertiesDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	DetailLayout = &InDetailLayout;
	FName EmitterCategoryName = TEXT("Emitter");
	IDetailCategoryBuilder& EmitterCategory = DetailLayout->EditCategory(EmitterCategoryName);

	TSharedRef<IPropertyHandle> EmitterName = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraEmitterProperties, EmitterName));
	TSharedRef<IPropertyHandle> EmitterEnabled = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraEmitterProperties, bIsEnabled));

	DetailLayout->HideProperty(EmitterName);
	DetailLayout->HideProperty(EmitterEnabled);
	
	TSharedRef<IPropertyHandle> UpdateScriptProps = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraEmitterProperties, UpdateScriptProps));
	TSharedRef<IPropertyHandle> SpawnScriptProps = DetailLayout->GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraEmitterProperties, SpawnScriptProps));

	BuildScriptProperties(SpawnScriptProps, TEXT("Spawn Script"), LOCTEXT("SpawnScriptDisplayName", "Spawn Script"));
	BuildScriptProperties(UpdateScriptProps, TEXT("Update Script"), LOCTEXT("UpdateScriptDisplayName", "Update Script"));
}

void FNiagaraEmitterPropertiesDetails::BuildScriptProperties(TSharedRef<IPropertyHandle> ScriptPropsHandle, FName Name, FText DisplayName)
{
	DetailLayout->HideProperty(ScriptPropsHandle);

	IDetailCategoryBuilder& ScriptCategory = DetailLayout->EditCategory(Name, DisplayName);

	//Script
	TSharedPtr<IPropertyHandle> ScriptHandle = ScriptPropsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEmitterScriptProperties, Script));
	ScriptCategory.AddProperty(ScriptHandle);

	//Constants
	bool bGenerateHeader = false;
	bool bDisplayResetToDefault = false;
	TSharedPtr<IPropertyHandle> Constants = ScriptPropsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEmitterScriptProperties, ExternalConstants));
	DetailLayout->HideProperty(Constants);
	//Scalar Constants.
	TSharedPtr<IPropertyHandle> ScalarConstants = Constants->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstants, ScalarConstants));
	TSharedRef<FDetailArrayBuilder> ScalarConstantsBuilder = MakeShareable(new FDetailArrayBuilder(ScalarConstants.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
	ScalarConstantsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterPropertiesDetails::OnGenerateScalarConstantEntry));
	ScriptCategory.AddCustomBuilder(ScalarConstantsBuilder);
	//Vector Constants.
	TSharedPtr<IPropertyHandle> VectorConstants = Constants->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstants, VectorConstants));
	TSharedRef<FDetailArrayBuilder> VectorConstantsBuilder = MakeShareable(new FDetailArrayBuilder(VectorConstants.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
	VectorConstantsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterPropertiesDetails::OnGenerateVectorConstantEntry));
	ScriptCategory.AddCustomBuilder(VectorConstantsBuilder);
	//Matrix Constants.
	TSharedPtr<IPropertyHandle> MatrixConstants = Constants->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstants, MatrixConstants));
	TSharedRef<FDetailArrayBuilder> MatrixConstantsBuilder = MakeShareable(new FDetailArrayBuilder(MatrixConstants.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
	MatrixConstantsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterPropertiesDetails::OnGenerateMatrixConstantEntry));
	ScriptCategory.AddCustomBuilder(MatrixConstantsBuilder);

	//Events
	bGenerateHeader = true;
	//Generators
	TSharedPtr<IPropertyHandle> EventGenerators = ScriptPropsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEmitterScriptProperties, EventGenerators));
	TSharedRef<FDetailArrayBuilder> EventGeneratorsBuilder = MakeShareable(new FDetailArrayBuilder(EventGenerators.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
	EventGeneratorsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterPropertiesDetails::OnGenerateEventGeneratorEntry));
	ScriptCategory.AddCustomBuilder(EventGeneratorsBuilder);
	//Receivers
	TSharedPtr<IPropertyHandle> EventReceivers = ScriptPropsHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEmitterScriptProperties, EventReceivers));
	TSharedRef<FDetailArrayBuilder> EventReceiversBuilder = MakeShareable(new FDetailArrayBuilder(EventReceivers.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
	EventReceiversBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraEmitterPropertiesDetails::OnGenerateEventReceiverEntry));
	ScriptCategory.AddCustomBuilder(EventReceiversBuilder);

}

void FNiagaraEmitterPropertiesDetails::OnGenerateScalarConstantEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedPtr<IPropertyHandle> ValueProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstant_Float, Value));
	TSharedPtr<IPropertyHandle> NameProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstant_Float, Name));

	FName DisplayName;
	NameProperty->GetValue(DisplayName);
	//Don't display system constants
	if (UNiagaraComponent::GetSystemConstants().Find(FNiagaraVariableInfo(DisplayName, ENiagaraDataType::Scalar)) == INDEX_NONE)
	{
		ChildrenBuilder.AddChildProperty(ValueProperty.ToSharedRef()).DisplayName(FText::FromName(DisplayName));
	}
}

void FNiagaraEmitterPropertiesDetails::OnGenerateVectorConstantEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedPtr<IPropertyHandle> ValueProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstant_Vector, Value));
	TSharedPtr<IPropertyHandle> NameProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstant_Vector, Name));

	FName DisplayName;
	NameProperty->GetValue(DisplayName);
	//Don't display system constants
	if (UNiagaraComponent::GetSystemConstants().Find(FNiagaraVariableInfo(DisplayName, ENiagaraDataType::Vector)) == INDEX_NONE)
	{
		ChildrenBuilder.AddChildProperty(ValueProperty.ToSharedRef()).DisplayName(FText::FromName(DisplayName));
	}
}

void FNiagaraEmitterPropertiesDetails::OnGenerateMatrixConstantEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedPtr<IPropertyHandle> ValueProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstant_Matrix, Value));
	TSharedPtr<IPropertyHandle> NameProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraConstant_Matrix, Name));

	FName DisplayName;
	NameProperty->GetValue(DisplayName);
	//Don't display system constants
	if (UNiagaraComponent::GetSystemConstants().Find(FNiagaraVariableInfo(DisplayName, ENiagaraDataType::Matrix)) == INDEX_NONE)
	{
		ChildrenBuilder.AddChildProperty(ValueProperty.ToSharedRef()).DisplayName(FText::FromName(DisplayName));
	}
}

void FNiagaraEmitterPropertiesDetails::OnGenerateEventGeneratorEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedPtr<IPropertyHandle> IDProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEventGeneratorProperties, ID));
	TSharedPtr<IPropertyHandle> NameProperty = IDProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraDataSetID, Name));

	FName DisplayName;
	NameProperty->GetValue(DisplayName);
	//IDetailPropertyRow& Row = ChildrenBuilder.AddChildProperty(ElementProperty).DisplayName(FText::FromName(DisplayName));

	IDetailGroup& GenGroup = ChildrenBuilder.AddChildGroup(DisplayName, FText::FromName(DisplayName));
	uint32 NumChildren = 0;
	if (ElementProperty->GetNumChildren(NumChildren) == FPropertyAccess::Success)
	{
		for (uint32 i = 0; i < NumChildren; ++i)
		{
			TSharedPtr<IPropertyHandle> Child = ElementProperty->GetChildHandle(i);
			//Dont add the ID. We just grab it's name for the name region of this property.
			if (Child.IsValid() && Child->GetProperty()->GetName() != GET_MEMBER_NAME_CHECKED(FNiagaraEventGeneratorProperties, ID).ToString())
			{
				GenGroup.AddPropertyRow(Child.ToSharedRef());
			}
		}
	}
}

void FNiagaraEmitterPropertiesDetails::OnGenerateEventReceiverEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	TSharedPtr<IPropertyHandle> NameProperty = ElementProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FNiagaraEventReceiverProperties, Name));

 	FName DisplayName;
 	NameProperty->GetValue(DisplayName);
// 	ChildrenBuilder.AddChildProperty(ElementProperty).DisplayName(FText::FromName(DisplayName));

	IDetailGroup& Group = ChildrenBuilder.AddChildGroup(DisplayName, FText::FromName(DisplayName));
	uint32 NumChildren = 0;
	if (ElementProperty->GetNumChildren(NumChildren) == FPropertyAccess::Success)
	{
		for (uint32 i = 0; i < NumChildren; ++i)
		{
			TSharedPtr<IPropertyHandle> Child = ElementProperty->GetChildHandle(i);
			//Dont add the ID. We just grab it's name for the name region of this property.
			if (Child.IsValid() && Child->GetProperty()->GetName() != GET_MEMBER_NAME_CHECKED(FNiagaraEventReceiverProperties, Name).ToString())
			{
				TSharedPtr<SWidget> NameWidget;
				TSharedPtr<SWidget> ValueWidget;
				FDetailWidgetRow DefaultDetailRow;
			
				IDetailPropertyRow& Row = Group.AddPropertyRow(Child.ToSharedRef());
				Row.GetDefaultWidgets(NameWidget, ValueWidget, DefaultDetailRow);
				Row.CustomWidget(true)
					.NameContent()
					[
						NameWidget.ToSharedRef()
					]
					.ValueContent()
					[
						ValueWidget.ToSharedRef()
					];
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

