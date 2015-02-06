// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/DataTable.h"

#define LOCTEXT_NAMESPACE "FDataTableCustomizationLayout"

/**
 * Customizes a DataTable asset to use a dropdown
 */
class FDataTableCustomizationLayout : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() 
	{
		return MakeShareable( new FDataTableCustomizationLayout );
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override
	{
		this->StructPropertyHandle = InStructPropertyHandle;

		HeaderRow
			.NameContent()
			[
				InStructPropertyHandle->CreatePropertyNameWidget( FText::GetEmpty(), FText::GetEmpty(), false )
			];
	}

	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override
	{
		/** Get all the existing property handles */
		DataTablePropertyHandle = InStructPropertyHandle->GetChildHandle( "DataTable" );
		RowNamePropertyHandle = InStructPropertyHandle->GetChildHandle( "RowName" );

		if( DataTablePropertyHandle->IsValidHandle() && RowNamePropertyHandle->IsValidHandle() )
		{
			/** Init the array of strings from the fname map */
			CurrentSelectedItem = InitWidgetContent();

			/** Edit the data table uobject as normal */
			StructBuilder.AddChildProperty( DataTablePropertyHandle.ToSharedRef() );
			FSimpleDelegate OnDataTableChangedDelegate = FSimpleDelegate::CreateSP( this, &FDataTableCustomizationLayout::OnDataTableChanged );
			DataTablePropertyHandle->SetOnPropertyValueChanged( OnDataTableChangedDelegate );

			/** Construct a combo box widget to select from a list of valid options */
			StructBuilder.AddChildContent( LOCTEXT( "DataTable_RowName", "Row Name" ) )
			.NameContent()
				[
					SNew( STextBlock )
					.Text( LOCTEXT( "DataTable_RowName", "Row Name" ) )
					.Font( StructCustomizationUtils.GetRegularFont() )
				]
			.ValueContent()
			.MaxDesiredWidth(0.0f) // don't constrain the combo button width
				[
					SAssignNew( RowNameComboButton, SComboButton )
					.ToolTipText( this, &FDataTableCustomizationLayout::GetRowNameComboBoxContentText )
					.OnGetMenuContent( this, &FDataTableCustomizationLayout::GetListContent )
					.ContentPadding( FMargin( 2.0f, 2.0f ) )
					.ButtonContent()
					[
						SNew( STextBlock )
						.Text( this, &FDataTableCustomizationLayout::GetRowNameComboBoxContentText )
					]
				];
		}
	}

private:
	/** Init the contents the combobox sources its data off */
	TSharedPtr<FString> InitWidgetContent()
	{
		TSharedPtr<FString> InitialValue = MakeShareable( new FString( LOCTEXT( "DataTable_None", "None" ).ToString() ) );;

		FName RowName;
		const FPropertyAccess::Result RowResult = RowNamePropertyHandle->GetValue( RowName );
		RowNames.Empty();
		
		/** Get the properties we wish to work with */
		UDataTable* DataTable = NULL;
		DataTablePropertyHandle->GetValue( ( UObject*& )DataTable );

		if( DataTable != NULL )
		{
			/** Extract all the row names from the RowMap */
			for( TMap<FName, uint8*>::TConstIterator Iterator( DataTable->RowMap ); Iterator; ++Iterator )
			{
				/** Create a simple array of the row names */
				TSharedRef<FString> RowNameItem = MakeShareable( new FString( Iterator.Key().ToString() ) );
				RowNames.Add( RowNameItem );

				/** Set the initial value to the currently selected item */
				if( Iterator.Key() == RowName )
				{
					InitialValue = RowNameItem;
				}
			}
		}

		/** Reset the initial value to ensure a valid entry is set */
		if ( RowResult != FPropertyAccess::MultipleValues )
		{
			FName NewValue = FName( **InitialValue );
			RowNamePropertyHandle->SetValue( NewValue );
		}

		return InitialValue;
	}

	/** Returns the ListView for the ComboButton */
	TSharedRef<SWidget> GetListContent();

	/** Delegate to refresh the drop down when the datatable changes */
	void OnDataTableChanged()
	{
		if( RowNameComboListView.IsValid() )
		{
			TSharedPtr<FString> InitialValue = InitWidgetContent();
			RowNameComboListView->SetSelection( InitialValue );
			RowNameComboListView->RequestListRefresh();
		}
	}

	/** Return the representation of the the row names to display */
	TSharedRef<ITableRow>  HandleRowNameComboBoxGenarateWidget( TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable )
	{
		return
			SNew( STableRow<TSharedPtr<FString>>, OwnerTable)
			[
				SNew( STextBlock ).Text( FText::FromString(*InItem) )
			];
	}

	/** Display the current selection */
	FText GetRowNameComboBoxContentText( ) const
	{
		FString RowNameValue;
		const FPropertyAccess::Result RowResult = RowNamePropertyHandle->GetValue( RowNameValue );
		if ( RowResult != FPropertyAccess::MultipleValues )
		{
			TSharedPtr<FString> SelectedRowName = CurrentSelectedItem;
			if ( SelectedRowName.IsValid() )
			{
				return FText::FromString(*SelectedRowName);
			}
			else
			{
				return LOCTEXT("DataTable_None", "None");
			}
		}
		return LOCTEXT( "MultipleValues", "Multiple Values" );
	}

	/** Update the root data on a change of selection */
	void OnSelectionChanged( TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo )
	{
		if( SelectedItem.IsValid() )
		{
			CurrentSelectedItem = SelectedItem; 
			FName NewValue = FName( **SelectedItem );
			RowNamePropertyHandle->SetValue( NewValue );

			// Close the combo
			RowNameComboButton->SetIsOpen( false );
		}
	}

	/** Called by Slate when the filter box changes text. */
	void OnFilterTextChanged( const FText& InFilterText )
	{
		FString CurrentFilterText = InFilterText.ToString();

		FName RowName;
		const FPropertyAccess::Result RowResult = RowNamePropertyHandle->GetValue( RowName );
		RowNames.Empty();

		/** Get the properties we wish to work with */
		UDataTable* DataTable = NULL;
		DataTablePropertyHandle->GetValue( ( UObject*& )DataTable );

		if( DataTable != NULL )
		{
			/** Extract all the row names from the RowMap */
			for( TMap<FName, uint8*>::TConstIterator Iterator( DataTable->RowMap ); Iterator; ++Iterator )
			{
				/** Create a simple array of the row names */
				FString RowString = Iterator.Key().ToString();
				if( CurrentFilterText == TEXT("") || RowString.Contains(CurrentFilterText) )
				{
					TSharedRef<FString> RowNameItem = MakeShareable( new FString( RowString ) );				
					RowNames.Add( RowNameItem );
				}
			}
		}
		RowNameComboListView->RequestListRefresh();
	}

	/** The comboButton objects */
	TSharedPtr<SComboButton> RowNameComboButton;
	TSharedPtr<SListView<TSharedPtr<FString> > > RowNameComboListView;
	TSharedPtr<FString> CurrentSelectedItem;	
	/** Handle to the struct properties being customized */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<IPropertyHandle> DataTablePropertyHandle;
	TSharedPtr<IPropertyHandle> RowNamePropertyHandle;
	/** A cached copy of strings to populate the combo box */
	TArray<TSharedPtr<FString> > RowNames;
};

#undef LOCTEXT_NAMESPACE
