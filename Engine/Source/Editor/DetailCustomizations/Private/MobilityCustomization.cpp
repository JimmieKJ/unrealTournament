// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "MobilityCustomization.h"

#define LOCTEXT_NAMESPACE "MobilityCustomization"

void FMobilityCustomization::CreateMobilityCustomization(IDetailCategoryBuilder& Category, TSharedPtr<IPropertyHandle> InMobilityHandle, uint8 RestrictedMobilityBits)
{
	MobilityHandle = InMobilityHandle;

	TSharedPtr<SUniformGridPanel> ButtonOptionsPanel;
		
	IDetailPropertyRow& MobilityRow = Category.AddProperty(MobilityHandle);
	MobilityRow.CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Mobility", "Mobility"))
		.ToolTipText(this, &FMobilityCustomization::GetMobilityToolTip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(0)
	[
		SAssignNew(ButtonOptionsPanel, SUniformGridPanel)
	];

	bool bShowStatic = !( RestrictedMobilityBits & StaticMobilityBitMask );
	bool bShowStationary = !( RestrictedMobilityBits & StationaryMobilityBitMask );

	int32 ColumnIndex = 0;

	if ( bShowStatic )
	{
		// Static Mobility
		ButtonOptionsPanel->AddSlot(0, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.Start")
			.IsChecked(this, &FMobilityCustomization::IsMobilityActive, EComponentMobility::Static)
			.OnCheckStateChanged(this, &FMobilityCustomization::OnMobilityChanged, EComponentMobility::Static)
			.ToolTipText(LOCTEXT("Mobility_Static_Tooltip", "A static object can't be changed in game.\n* Allows Baked Lighting\n* Fastest Rendering"))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Mobility.Static"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(6, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Static", "Static"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(this, &FMobilityCustomization::GetMobilityTextColor, EComponentMobility::Static)
				]
			]
		];

		ColumnIndex++;
	}

	// Stationary Mobility
	if ( bShowStationary )
	{
		ButtonOptionsPanel->AddSlot(ColumnIndex, 0)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FMobilityCustomization::IsMobilityActive, EComponentMobility::Stationary)
			.Style(FEditorStyle::Get(), ( ColumnIndex == 0 ) ? "Property.ToggleButton.Start" : "Property.ToggleButton.Middle")
			.OnCheckStateChanged(this, &FMobilityCustomization::OnMobilityChanged, EComponentMobility::Stationary)
			.ToolTipText(LOCTEXT("Mobility_Stationary_Tooltip", "A stationary light will only have its shadowing and bounced lighting from static geometry baked by Lightmass, all other lighting will be dynamic.  It can change color and intensity in game.\n* Can't Move\n* Allows Partial Baked Lighting\n* Dynamic Shadows"))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Mobility.Stationary"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(6, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Stationary", "Stationary"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(this, &FMobilityCustomization::GetMobilityTextColor, EComponentMobility::Stationary)
				]
			]
		];

		ColumnIndex++;
	}

	// Movable Mobility
	ButtonOptionsPanel->AddSlot(ColumnIndex, 0)
	[
		SNew(SCheckBox)
		.IsChecked(this, &FMobilityCustomization::IsMobilityActive, EComponentMobility::Movable)
		.Style(FEditorStyle::Get(), ( ColumnIndex == 0 ) ? "Property.ToggleButton" : "Property.ToggleButton.End")
		.OnCheckStateChanged(this, &FMobilityCustomization::OnMobilityChanged, EComponentMobility::Movable)
		.ToolTipText(LOCTEXT("Mobility_Movable_Tooltip", "Movable objects can be moved and changed in game.\n* Totally Dynamic\n* Allows Dynamic Shadows\n* Slowest Rendering"))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 2)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("Mobility.Movable"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(6, 2)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Movable", "Movable"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(this, &FMobilityCustomization::GetMobilityTextColor, EComponentMobility::Movable)
			]
		]
	];
}

ECheckBoxState FMobilityCustomization::IsMobilityActive(EComponentMobility::Type InMobility) const
{
	if (MobilityHandle.IsValid())
	{
		uint8 MobilityByte;
		MobilityHandle->GetValue(MobilityByte);

		return MobilityByte == InMobility ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

FSlateColor FMobilityCustomization::GetMobilityTextColor(EComponentMobility::Type InMobility) const
{
	if (MobilityHandle.IsValid())
	{
		uint8 MobilityByte;
		MobilityHandle->GetValue(MobilityByte);

		return MobilityByte == InMobility ? FSlateColor(FLinearColor(0, 0, 0)) : FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
	}

	return FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
}

void FMobilityCustomization::OnMobilityChanged(ECheckBoxState InCheckedState, EComponentMobility::Type InMobility)
{
	if (MobilityHandle.IsValid() && InCheckedState == ECheckBoxState::Checked)
	{
		MobilityHandle->SetValue((uint8)InMobility);
	}
}

FText FMobilityCustomization::GetMobilityToolTip() const
{
	if (MobilityHandle.IsValid())
	{
		return MobilityHandle->GetToolTipText();
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE

