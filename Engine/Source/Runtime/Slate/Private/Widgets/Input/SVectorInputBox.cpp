// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SVectorInputBox.h"
#include "SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "SVectorInputBox"


void SVectorInputBox::Construct( const FArguments& InArgs )
{
	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	ChildSlot
	[
		HorizontalBox
	];

	ConstructX( InArgs, HorizontalBox );
	ConstructY( InArgs, HorizontalBox );
	ConstructZ( InArgs, HorizontalBox );
}

void SVectorInputBox::ConstructX( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox )
{	
	FLinearColor LabelColor;
	LabelColor = InArgs._bColorAxisLabels ?  SNumericEntryBox<float>::RedLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f);

	HorizontalBox->AddSlot()
	.VAlign( VAlign_Center )
	.FillWidth( 1.0f )
	.Padding( 0.0f, 1.0f, 2.0f, 1.0f )
	[
		SNew( SNumericEntryBox<float> )
		.Font( InArgs._Font )
		.Value( InArgs._X )
		.OnValueChanged( InArgs._OnXChanged )
		.OnValueCommitted( InArgs._OnXCommitted )		
		.ToolTipText( LOCTEXT("X_ToolTip", "X Value") )
		.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
		.LabelPadding(0)
		.ContextMenuExtender( InArgs._ContextMenuExtenderX )
		.TypeInterface(InArgs._TypeInterface)
		.Label()
		[
			SNumericEntryBox<float>::BuildLabel( LOCTEXT("X_Label", "X"), FLinearColor::White, LabelColor )
		]
	];
	
}


void SVectorInputBox::ConstructY( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox )
{
	FLinearColor LabelColor;
	LabelColor = InArgs._bColorAxisLabels ?  SNumericEntryBox<float>::GreenLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f);

	HorizontalBox->AddSlot()
	.VAlign( VAlign_Center )
	.FillWidth( 1.0f )
	.Padding( 0.0f, 1.0f, 2.0f, 1.0f )
	[
		SNew( SNumericEntryBox<float> )
		.Font( InArgs._Font )
		.Value( InArgs._Y )
		.OnValueChanged( InArgs._OnYChanged )
		.OnValueCommitted( InArgs._OnYCommitted )
		.ToolTipText( LOCTEXT("Y_ToolTip", "Y Value") )
		.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
		.LabelPadding(0)
		.ContextMenuExtender( InArgs._ContextMenuExtenderY )
		.TypeInterface(InArgs._TypeInterface)
		.Label()
		[
			SNumericEntryBox<float>::BuildLabel( LOCTEXT("Y_Label", "Y"), FLinearColor::White, LabelColor )
		]
	];

}

void SVectorInputBox::ConstructZ( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox )
{
	FLinearColor LabelColor;
	LabelColor = InArgs._bColorAxisLabels ?  SNumericEntryBox<float>::BlueLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f);

	HorizontalBox->AddSlot()
	.VAlign( VAlign_Center )
	.FillWidth( 1.0f )
	.Padding( 0.0f, 1.0f, 0.0f, 1.0f )
	[
		SNew( SNumericEntryBox<float> )
		.Font( InArgs._Font )
		.Value( InArgs._Z )
		.OnValueChanged( InArgs._OnZChanged )
		.OnValueCommitted( InArgs._OnZCommitted )
		.ToolTipText( LOCTEXT("Z_ToolTip", "Z Value") )
		.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
		.LabelPadding(0)
		.ContextMenuExtender( InArgs._ContextMenuExtenderZ )
		.TypeInterface(InArgs._TypeInterface)
		.Label()
		[
			SNumericEntryBox<float>::BuildLabel( LOCTEXT("Z_Label", "Z"), FLinearColor::White, LabelColor )
		]
	];
}

#undef LOCTEXT_NAMESPACE
