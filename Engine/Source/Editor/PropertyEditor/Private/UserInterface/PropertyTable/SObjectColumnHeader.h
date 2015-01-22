// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SColumnHeader.h"
#include "BooleanPropertyTableCellPresenter.h"
#include "ColorPropertyTableCellPresenter.h"
#include "TextPropertyTableCellPresenter.h"
#include "IPropertyTableUtilities.h"

class SObjectColumnHeader : public SColumnHeader
{
	public:

	SLATE_BEGIN_ARGS( SObjectColumnHeader )
		: _Style( TEXT("PropertyTable") )
		, _Customization()
	{}
		SLATE_ARGUMENT( FName, Style )
		SLATE_ARGUMENT( TSharedPtr< IPropertyTableCustomColumn >, Customization )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef< class IPropertyTableColumn >& InPropertyTableColumn, const TSharedRef< class IPropertyTableUtilities >& InPropertyUtilities )
	{
		Style = InArgs._Style;

		SColumnHeader::FArguments ColumnArgs;
		ColumnArgs.Style( Style );
		ColumnArgs.Customization( Customization );

		SColumnHeader::Construct( ColumnArgs, InPropertyTableColumn, InPropertyUtilities );
	}

	virtual TSharedRef< SWidget > GenerateCell( const TSharedRef< class IPropertyTableRow >& PropertyTableRow ) override
	{
		TSharedRef< IPropertyTableCell > Cell = Column->GetCell( PropertyTableRow );

		TSharedPtr< IPropertyTableCellPresenter > CellPresenter( NULL );

		if ( Customization.IsValid() )
		{
			CellPresenter = Customization->CreateCellPresenter( Cell, Utilities.ToSharedRef(), Style );
		}

		if ( !CellPresenter.IsValid() && Cell->IsBound() )
		{
			TSharedRef< FPropertyEditor > PropertyEditor = FPropertyEditor::Create( Cell->GetNode().ToSharedRef(), Utilities.ToSharedRef() );

			TWeakObjectPtr< UProperty > Property = PropertyTableRow->GetDataSource()->AsPropertyPath()->GetLeafMostProperty().Property;

			if( Property->IsA( UBoolProperty::StaticClass() ) )
			{
				CellPresenter = MakeShareable( new FBooleanPropertyTableCellPresenter( PropertyEditor ) );
			}
			else if ( Cast<const UStructProperty>(Property.Get()) && (Cast<const UStructProperty>(Property.Get())->Struct->GetFName()==NAME_Color || Cast<const UStructProperty>(Property.Get())->Struct->GetFName()==NAME_LinearColor) )
			{
				CellPresenter = MakeShareable( new FColorPropertyTableCellPresenter( PropertyEditor, Utilities.ToSharedRef() ) );
			}
			else
			{
				CellPresenter = MakeShareable( new FTextPropertyTableCellPresenter( PropertyEditor, Utilities.ToSharedRef() ) );
			}
		}

		return SNew( SPropertyTableCell, Cell )
			.Presenter( CellPresenter )
			.Style( Style );
	}


private:

	FName Style;
};