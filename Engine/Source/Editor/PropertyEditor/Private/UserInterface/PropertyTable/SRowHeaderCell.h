// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PropertyEditorHelpers.h"
#include "IPropertyTableUtilities.h"
#include "PropertyNode.h"
#include "ItemPropertyNode.h"
#include "ObjectPropertyNode.h"
#include "PropertyEditor.h"

#include "IPropertyTableColumn.h"
#include "IPropertyTableRow.h"
#include "IPropertyTableCell.h"
#include "IPropertyTable.h"

class SRowHeaderCell : public SCompoundWidget
{
	public:

	SLATE_BEGIN_ARGS( SRowHeaderCell )
		: _Style( TEXT("PropertyTable") )
	{}
		SLATE_ARGUMENT( FName, Style )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InViewModel			The UI logic not specific to slate
	 */
	void Construct( const FArguments& InArgs, const TSharedRef< class IPropertyTableCell >& InCell, const TSharedPtr< FPropertyEditor >& PropertyEditor )
	{
		Style = InArgs._Style;
		Cell = InCell;

		TArray< EPropertyButton::Type > RequiredButtons;
		if ( PropertyEditor.IsValid() )
		{
			PropertyEditorHelpers::GetRequiredPropertyButtons( PropertyEditor->GetPropertyNode(), RequiredButtons );
		}

		TSharedRef< SWidget > Content = 
			SNew(SImage)
			.Image( FEditorStyle::GetBrush("ContentBrowser.ContentDirty") )
			.Visibility( this, &SRowHeaderCell::GetDirtyImageVisibility );

		if ( RequiredButtons.Contains( EPropertyButton::Insert_Delete_Duplicate ) )
		{
			Editor = PropertyEditor;
			Content =
				SNew( SOverlay )
				+SOverlay::Slot()
				[
					Content
				]
				
				+SOverlay::Slot()
				[
					PropertyEditorHelpers::MakePropertyButton( EPropertyButton::Insert_Delete_Duplicate, PropertyEditor.ToSharedRef() )
				];
		}

		ChildSlot
		[
			SNew( SBox )
			.HeightOverride( 20.0f )
			[
				SNew( SBorder )
				.BorderImage( this, &SRowHeaderCell::GetBorder )
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				.Content()
				[
					Content
				]
			]
		];
	}

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		const TSharedRef< IPropertyTable > Table = Cell->GetTable();
		Table->SetLastClickedCell( Cell );

		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& MouseEvent ) override
	{
		const TSharedRef< IPropertyTable > Table = Cell->GetTable();
		Table->SetLastClickedCell( Cell );

		return FReply::Unhandled();
	}


private:

	const FSlateBrush* GetBorder() const 
	{
		return FEditorStyle::GetBrush( Style, ".RowHeader.Background" );
	}

	EVisibility GetDirtyImageVisibility() const
	{
		const TWeakObjectPtr< UObject > Object = Cell->GetObject();

		if ( !Object.IsValid() )
		{
			return EVisibility::Hidden;
		}

		UPackage* Package = Object->GetOutermost();
		return Package != NULL && Package->IsDirty() ? EVisibility::Visible : EVisibility::Hidden;
	}


private:

	TSharedPtr< IPropertyTableCell > Cell;
	TSharedPtr< FPropertyEditor > Editor;
	FName Style;
};