// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "PropertyEditorPrivatePCH.h"
#include "SResetToDefaultPropertyEditor.h"
#include "PropertyEditor.h"
#include "IDetailPropertyRow.h"

#define LOCTEXT_NAMESPACE "ResetToDefaultPropertyEditor"

void SResetToDefaultPropertyEditor::Construct( const FArguments& InArgs, const TSharedRef<FPropertyEditor>& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;
	NonVisibleState = InArgs._NonVisibleState;
	bValueDiffersFromDefault = false;
	OptionalCustomResetToDefault = InArgs._CustomResetToDefault;

	if (OptionalCustomResetToDefault.IsSet())
	{
		ChildSlot
		[
			SNew(SButton)
			.ToolTipText(NSLOCTEXT("PropertyEditor", "ResetToDefaultToolTip", "Reset to Default"))
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.ContentPadding(0.0f) 
			.Visibility( this, &SResetToDefaultPropertyEditor::GetDiffersFromDefaultAsVisibility )
			.OnClicked( this, &SResetToDefaultPropertyEditor::OnCustomResetClicked )
			.Content()
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		];
	}
	else
	{
		// Indicator for a value that differs from default. Also offers the option to reset to default.
		ChildSlot
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(this, &SResetToDefaultPropertyEditor::GetResetToolTip)
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.ContentPadding(0) 
			.Visibility( this, &SResetToDefaultPropertyEditor::GetDiffersFromDefaultAsVisibility )
			.OnClicked( this, &SResetToDefaultPropertyEditor::OnDefaultResetClicked )
			.Content()
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
			]
		];
	}
}

void SResetToDefaultPropertyEditor::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (OptionalCustomResetToDefault.IsSet())
	{
		bValueDiffersFromDefault = OptionalCustomResetToDefault.GetValue().IsResetToDefaultVisible(PropertyEditor->GetPropertyHandle());
	}
	else
	{
		bValueDiffersFromDefault = PropertyEditor->IsResetToDefaultAvailable();
	}
}

FText SResetToDefaultPropertyEditor::GetResetToolTip() const
{
	FString Tooltip;
	Tooltip = NSLOCTEXT("PropertyEditor", "ResetToDefaultToolTip", "Reset to Default").ToString();

	if( !PropertyEditor->IsEditConst() && PropertyEditor->ValueDiffersFromDefault() )
	{
		Tooltip += "\n";
		Tooltip += PropertyEditor->GetResetToDefaultLabel().ToString();
	}

	return FText::FromString(Tooltip);
}

FReply SResetToDefaultPropertyEditor::OnDefaultResetClicked()
{
	PropertyEditor->ResetToDefault();

	return FReply::Handled();
}

FReply SResetToDefaultPropertyEditor::OnCustomResetClicked()
{
	PropertyEditor->CustomResetToDefault(OptionalCustomResetToDefault.GetValue());

	return FReply::Handled();
}

EVisibility SResetToDefaultPropertyEditor::GetDiffersFromDefaultAsVisibility() const
{
	return bValueDiffersFromDefault ? EVisibility::Visible : NonVisibleState;
}

#undef LOCTEXT_NAMESPACE