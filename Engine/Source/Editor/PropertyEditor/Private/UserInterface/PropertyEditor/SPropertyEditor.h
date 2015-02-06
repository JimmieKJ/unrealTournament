// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PropertyEditor.h"
#include "PropertyEditorConstants.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

class SPropertyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPropertyEditor )
		: _Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) ) 
	{}
	SLATE_ATTRIBUTE( FSlateFontInfo, Font )
		SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor )
	{
		PropertyEditor = InPropertyEditor;

		if( ShouldShowValue( InPropertyEditor ) )
		{
			ChildSlot
			[
				// Make a read only text box so that copy still works
				SNew( SEditableTextBox )
				.Text( InPropertyEditor, &FPropertyEditor::GetValueAsText )
				.ToolTipText( InPropertyEditor, &FPropertyEditor::GetValueAsText )
				.Font( InArgs._Font )
				.IsReadOnly( true )
			];
		}
	}

	void GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth )
	{
		OutMinDesiredWidth = 125.0f;
		OutMaxDesiredWidth = 0.0f;

		if( PropertyEditor.IsValid() )
		{
			const UProperty* Property = PropertyEditor->GetProperty();
			if( Property && Property->IsA<UStructProperty>() )
			{
				// Struct headers with nothing in them have no min width
				OutMinDesiredWidth = 0;
				OutMaxDesiredWidth = 130.0f;
			}
			
		}
	}
private:
	bool ShouldShowValue( const TSharedRef< class FPropertyEditor >& InPropertyEditor ) const 
	{
		return PropertyEditor->GetProperty() && !PropertyEditor->GetProperty()->IsA<UStructProperty>();
	}
private:
	TSharedPtr< class FPropertyEditor > PropertyEditor;
};

#undef LOCTEXT_NAMESPACE