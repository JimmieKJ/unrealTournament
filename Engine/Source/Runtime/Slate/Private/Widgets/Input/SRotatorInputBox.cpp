// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SRotatorInputBox.h"
#include "SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "SRotatorInputBox"


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SRotatorInputBox::Construct( const SRotatorInputBox::FArguments& InArgs )
{
	FLinearColor LabelColorX;
	FLinearColor LabelColorY;
	FLinearColor LabelColorZ;
	LabelColorX = InArgs._bColorAxisLabels ? SNumericEntryBox<float>::RedLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f);
	LabelColorY = InArgs._bColorAxisLabels ? SNumericEntryBox<float>::GreenLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f);
	LabelColorZ = InArgs._bColorAxisLabels ? SNumericEntryBox<float>::BlueLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f);

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.FillWidth( 1.0f )
		.Padding( 0.0f, 1.0f, 2.0f, 1.0f )
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(InArgs._AllowSpin)
			.MinSliderValue(0.0f)
			.MaxSliderValue(359.999f)
			.LabelPadding(0)
			.Label()
			[
				SNumericEntryBox<float>::BuildLabel( LOCTEXT("Roll_Label", "X"), FLinearColor::White, SNumericEntryBox<float>::RedLabelBackgroundColor )
			]
			.Font( InArgs._Font )
			.Value( InArgs._Roll )
			.OnValueChanged( InArgs._OnRollChanged )
			.OnValueCommitted( InArgs._OnRollCommitted )
			.OnBeginSliderMovement( InArgs._OnBeginSliderMovement )
			.OnEndSliderMovement( InArgs._OnEndSliderMovement )
			.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
			.ToolTipText( LOCTEXT("Roll_ToolTip", "Roll Value") )
			.TypeInterface(InArgs._TypeInterface)
		]
		+SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.FillWidth( 1.0f )
		.Padding( 0.0f, 1.0f, 2.0f, 1.0f )
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(InArgs._AllowSpin)
			.MinSliderValue(0.0f)
			.MaxSliderValue(359.999f)
			.LabelPadding(0)
			.Label()
			[
				SNumericEntryBox<float>::BuildLabel( LOCTEXT("Pitch_Label", "Y"), FLinearColor::White, SNumericEntryBox<float>::GreenLabelBackgroundColor )
			]
			.Font( InArgs._Font )
			.Value( InArgs._Pitch )
			.OnValueChanged( InArgs._OnPitchChanged )
			.OnValueCommitted( InArgs._OnPitchCommitted )
			.OnBeginSliderMovement( InArgs._OnBeginSliderMovement )
			.OnEndSliderMovement( InArgs._OnEndSliderMovement )
			.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
			.ToolTipText( LOCTEXT("Pitch_ToolTip", "Pitch Value") )
			.TypeInterface(InArgs._TypeInterface)
		]
		+SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.FillWidth( 1.0f )
		.Padding( 0.0f, 1.0f, 0.0f, 1.0f )
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(InArgs._AllowSpin)
			.MinSliderValue(0.0f)
			.MaxSliderValue(359.999f)
			.LabelPadding(0)
			.Label()
			[
				SNumericEntryBox<float>::BuildLabel( LOCTEXT("Yaw_Label", "Z"), FLinearColor::White, SNumericEntryBox<float>::BlueLabelBackgroundColor )
			]
			.Font( InArgs._Font )
			.Value( InArgs._Yaw )
			.OnValueChanged( InArgs._OnYawChanged )
			.OnValueCommitted( InArgs._OnYawCommitted )
			.OnBeginSliderMovement( InArgs._OnBeginSliderMovement )
			.OnEndSliderMovement( InArgs._OnEndSliderMovement )
			.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
			.ToolTipText( LOCTEXT("Yaw_ToolTip", "Yaw Value") )
			.TypeInterface(InArgs._TypeInterface)
		]
	];

}

#undef LOCTEXT_NAMESPACE

