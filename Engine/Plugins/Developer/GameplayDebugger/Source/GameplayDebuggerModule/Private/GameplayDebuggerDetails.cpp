// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPluginPrivatePCH.h"
#if WITH_EDITOR
#include "UnrealEd.h"
#include "PropertyEditing.h"
#include "PropertyCustomizationHelpers.h"
#include "GameplayDebuggerDetails.h"
#include "GameplayDebuggerModuleSettings.h"

#define LOCTEXT_NAMESPACE "InputStructCustomization"

namespace InputConstants
{
	const FMargin PropertyPadding(2.0f, 0.0f, 2.0f, 0.0f);
	const float TextBoxWidth = 200.0f;
	const float ScaleBoxWidth = 50.0f;
}

TSharedRef<IPropertyTypeCustomization> FGameplayDebuggerInputConfigDetails::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerInputConfigDetails);
}

void FGameplayDebuggerInputConfigDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{

}

void FGameplayDebuggerInputConfigDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> KeyHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputConfig, Key));
	TSharedPtr<IPropertyHandle> ShiftHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputConfig, bShift));
	TSharedPtr<IPropertyHandle> CtrlHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputConfig, bCtrl));
	TSharedPtr<IPropertyHandle> AltHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputConfig, bAlt));
	TSharedPtr<IPropertyHandle> CmdHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FInputConfig, bCmd));

	StructBuilder.AddChildContent(LOCTEXT("KeySearchStr", "Key"))
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(TOptional<float>())
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(InputConstants::PropertyPadding)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(InputConstants::TextBoxWidth)
				[
					StructBuilder.GenerateStructValueWidget(KeyHandle.ToSharedRef())
				]
			]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					ShiftHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					ShiftHandle->CreatePropertyValueWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CtrlHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CtrlHandle->CreatePropertyValueWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					AltHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					AltHandle->CreatePropertyValueWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CmdHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CmdHandle->CreatePropertyValueWidget()
				]
		];
}


/************************************************************************/
/*                                                                      */
/************************************************************************/
TSharedRef<IPropertyTypeCustomization> FGameplayDebuggerActionInputConfigDetails::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerActionInputConfigDetails);
}

void FGameplayDebuggerActionInputConfigDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FGameplayDebuggerActionInputConfigDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ActionNameHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputConfig, ActionName));
	TSharedPtr<IPropertyHandle> KeyHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputConfig, Key));
	TSharedPtr<IPropertyHandle> ShiftHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputConfig, bShift));
	TSharedPtr<IPropertyHandle> CtrlHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputConfig, bCtrl));
	TSharedPtr<IPropertyHandle> AltHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputConfig, bAlt));
	TSharedPtr<IPropertyHandle> CmdHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputConfig, bCmd));

	StructBuilder.AddChildContent(LOCTEXT("KeySearchStr", "Key"))
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(TOptional<float>())
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(InputConstants::TextBoxWidth)
					[
						StructBuilder.GenerateStructValueWidget(KeyHandle.ToSharedRef())
					]
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					ShiftHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					ShiftHandle->CreatePropertyValueWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CtrlHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CtrlHandle->CreatePropertyValueWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					AltHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					AltHandle->CreatePropertyValueWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CmdHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					CmdHandle->CreatePropertyValueWidget()
				]
		];
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
TSharedRef<IPropertyTypeCustomization> FGameplayDebuggerCategorySettingsDetails::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategorySettingsDetails);
}

void FGameplayDebuggerCategorySettingsDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{

}

void FGameplayDebuggerCategorySettingsDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> CategoryNameHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayDebuggerCategorySettings, CategoryName));
	TSharedPtr<IPropertyHandle> PIEHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayDebuggerCategorySettings, bPIE));
	TSharedPtr<IPropertyHandle> SimulateHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayDebuggerCategorySettings, bSimulate));
	TSharedPtr<IPropertyHandle> ClassesArrayHandle = InStructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGameplayDebuggerCategorySettings, ArrayOfClasses));

	FText NameOverride;
	CategoryNameHandle->GetValueAsDisplayText(NameOverride);

	StructBuilder.AddChildContent(LOCTEXT("KeySearchStr", "Key"))
		.NameContent()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			InStructPropertyHandle->CreatePropertyNameWidget(NameOverride)
		]
		.ValueContent()
		.MaxDesiredWidth(TOptional<float>())
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.AutoWidth()
				[
					PIEHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					PIEHandle->CreatePropertyValueWidget()
				]
#if 0 /** Separate settings for Simulate - not used, disabled for now */
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SimulateHandle->CreatePropertyNameWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(InputConstants::PropertyPadding)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SimulateHandle->CreatePropertyValueWidget()
				]
#endif
		];

#if 0 /** debug details */
		const bool bGenerateHeader = true, bDisplayResetToDefault = false;
		TSharedRef<FDetailArrayBuilder> ClassesArrayBuilder = MakeShareable(new FDetailArrayBuilder(ClassesArrayHandle.ToSharedRef(), bGenerateHeader, bDisplayResetToDefault));
		ClassesArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda(
			[](TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
		{
			ChildrenBuilder.AddChildProperty(ElementProperty);
		}
		));

		const bool bForAdvanced = false;
		StructBuilder.AddChildCustomBuilder(ClassesArrayBuilder);
#endif
}

#undef LOCTEXT_NAMESPACE
#endif //WITH_EDITOR
