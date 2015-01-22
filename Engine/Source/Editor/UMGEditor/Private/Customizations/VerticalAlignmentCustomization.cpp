// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "PropertyEditing.h"
#include "ObjectEditorUtils.h"
#include "WidgetGraphSchema.h"
#include "ScopedTransaction.h"
#include "BlueprintEditorUtils.h"

#include "VerticalAlignmentCustomization.h"

#define LOCTEXT_NAMESPACE "UMG"

void FVerticalAlignmentCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const FMargin OuterPadding(2);
	const FMargin ContentPadding(2);

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew( SCheckBox )
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(LOCTEXT("VAlignTop", "Vertically Align Top"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FVerticalAlignmentCustomization::HandleCheckStateChanged, PropertyHandle, VAlign_Top)
			.IsChecked(this, &FVerticalAlignmentCustomization::GetCheckState, PropertyHandle, VAlign_Top)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("VerticalAlignment_Top"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(LOCTEXT("VAlignCenter", "Vertically Align Center"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FVerticalAlignmentCustomization::HandleCheckStateChanged, PropertyHandle, VAlign_Center)
			.IsChecked(this, &FVerticalAlignmentCustomization::GetCheckState, PropertyHandle, VAlign_Center)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("VerticalAlignment_Center"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(LOCTEXT("VAlignBottom", "Vertically Align Bottom"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FVerticalAlignmentCustomization::HandleCheckStateChanged, PropertyHandle, VAlign_Bottom)
			.IsChecked(this, &FVerticalAlignmentCustomization::GetCheckState, PropertyHandle, VAlign_Bottom)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("VerticalAlignment_Bottom"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(OuterPadding)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(LOCTEXT("VAlignFill", "Vertically Align Fill"))
			.Padding(ContentPadding)
			.OnCheckStateChanged(this, &FVerticalAlignmentCustomization::HandleCheckStateChanged, PropertyHandle, VAlign_Fill)
			.IsChecked(this, &FVerticalAlignmentCustomization::GetCheckState, PropertyHandle, VAlign_Fill)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("VerticalAlignment_Fill"))
			]
		]
	];
}

void FVerticalAlignmentCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FVerticalAlignmentCustomization::HandleCheckStateChanged(ECheckBoxState InCheckboxState, TSharedRef<IPropertyHandle> PropertyHandle, EVerticalAlignment ToAlignment)
{
	PropertyHandle->SetValue((uint8)ToAlignment);
}

ECheckBoxState FVerticalAlignmentCustomization::GetCheckState(TSharedRef<IPropertyHandle> PropertyHandle, EVerticalAlignment ForAlignment) const
{
	uint8 Value;
	if ( PropertyHandle->GetValue(Value) )
	{
		return Value == ForAlignment ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
