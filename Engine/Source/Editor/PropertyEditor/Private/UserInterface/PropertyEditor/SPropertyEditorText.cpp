// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "SPropertyEditorText.h"
#include "PropertyNode.h"
#include "ObjectPropertyNode.h"
#include "PropertyEditor.h"
#include "PropertyEditorHelpers.h"


void SPropertyEditorText::Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor )
{
	PropertyEditor = InPropertyEditor;

	const UProperty* Property = InPropertyEditor->GetProperty();

	TSharedPtr<SHorizontalBox> HorizontalBox;

	bIsMultiLine = Property->GetBoolMetaData("MultiLine");
	if(bIsMultiLine)
	{
		ChildSlot
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(PrimaryWidget, SMultiLineEditableTextBox)
				.Text(InPropertyEditor, &FPropertyEditor::GetValueAsText)
				.Font(InArgs._Font)
				.SelectAllTextWhenFocused(false)
				.ClearKeyboardFocusOnCommit(false)
				.OnTextCommitted(this, &SPropertyEditorText::OnTextCommitted)
				.SelectAllTextOnCommit(false)
				.IsReadOnly(this, &SPropertyEditorText::IsReadOnly)
				.AutoWrapText(true)
				.ModiferKeyForNewLine(EModifierKey::Shift)
			]
		];
	}
	else
	{
		ChildSlot
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew( PrimaryWidget, SEditableTextBox )
				.Text( InPropertyEditor, &FPropertyEditor::GetValueAsText )
				.Font( InArgs._Font )
				.SelectAllTextWhenFocused( true )
				.ClearKeyboardFocusOnCommit(false)
				.OnTextCommitted( this, &SPropertyEditorText::OnTextCommitted )
				.SelectAllTextOnCommit( true )
				.IsReadOnly(this, &SPropertyEditorText::IsReadOnly)
			]
		];
	}

	if( InPropertyEditor->PropertyIsA( UObjectPropertyBase::StaticClass() ) )
	{
		// Object properties should display their entire text in a tooltip
		PrimaryWidget->SetToolTipText( TAttribute<FText>( InPropertyEditor, &FPropertyEditor::GetValueAsText ) );
	}

	SetEnabled( TAttribute<bool>( this, &SPropertyEditorText::CanEdit ) );
}

void SPropertyEditorText::GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
{
	if(bIsMultiLine)
	{
		OutMinDesiredWidth = 250.0f;
	}
	else
	{
		OutMinDesiredWidth = 125.0f;
	}
	
	OutMaxDesiredWidth = 600.0f;
}

bool SPropertyEditorText::Supports( const TSharedRef< FPropertyEditor >& InPropertyEditor )
{
	const TSharedRef< FPropertyNode > PropertyNode = InPropertyEditor->GetPropertyNode();
	const UProperty* Property = InPropertyEditor->GetProperty();

	if(	!PropertyNode->HasNodeFlags(EPropertyNodeFlags::EditInline)
		&&	( (Property->IsA(UNameProperty::StaticClass()) && Property->GetFName() != NAME_InitialState)
		||	Property->IsA(UStrProperty::StaticClass())
		||	Property->IsA(UTextProperty::StaticClass())
		||	(Property->IsA(UObjectPropertyBase::StaticClass()) && !Property->HasAnyPropertyFlags(CPF_InstancedReference))
		||	Property->IsA(UInterfaceProperty::StaticClass())
		||	Property->IsA(UMapProperty::StaticClass())
		) )
	{
		return true;
	}

	return false;
}

void SPropertyEditorText::OnTextCommitted( const FText& NewText, ETextCommit::Type /*CommitInfo*/ )
{
	const TSharedRef< FPropertyNode > PropertyNode = PropertyEditor->GetPropertyNode();
	const TSharedRef< IPropertyHandle > PropertyHandle = PropertyEditor->GetPropertyHandle();

	PropertyHandle->SetValueFromFormattedString( NewText.ToString() );
}

bool SPropertyEditorText::SupportsKeyboardFocus() const
{
	return PrimaryWidget.IsValid() && PrimaryWidget->SupportsKeyboardFocus();
}

FReply SPropertyEditorText::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	// Forward keyboard focus to our editable text widget
	return FReply::Handled().SetUserFocus(PrimaryWidget.ToSharedRef(), InFocusEvent.GetCause());
}

void SPropertyEditorText::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	const float CurrentHeight = AllottedGeometry.GetLocalSize().Y;
	if (bIsMultiLine && PreviousHeight.IsSet() && PreviousHeight.GetValue() != CurrentHeight)
	{
		PropertyEditor->RequestRefresh();
	}
	PreviousHeight = CurrentHeight;
}

bool SPropertyEditorText::CanEdit() const
{
	return PropertyEditor.IsValid() ? !PropertyEditor->IsEditConst() : true;
}

bool SPropertyEditorText::IsReadOnly() const
{
	return !CanEdit();
}
