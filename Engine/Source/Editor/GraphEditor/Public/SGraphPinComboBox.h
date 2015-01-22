// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A delegate type invoked when a selection changes somewhere. */
DECLARE_DELEGATE_RetVal_OneParam(FString, FGetComboItemDisplayString, int32);

//Class implementation to create combo box
class GRAPHEDITOR_API SPinComboBox : public SCompoundWidget
{
public:
	typedef SListView< TSharedPtr<int32> > SComboList;
	typedef TSlateDelegates< TSharedPtr<int32> >::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS( SPinComboBox ){}		
		SLATE_ATTRIBUTE( TArray< TSharedPtr<int32> >, ComboItemList )
		SLATE_ATTRIBUTE( FString, VisibleText )
		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )
		SLATE_EVENT( FGetComboItemDisplayString, OnGetDisplayName )
	SLATE_END_ARGS()

	//Construct combo box using combo button and combo list
	void Construct( const FArguments& InArgs );

	//Function to return currently selected string
	const TSharedPtr<int32> GetSelectedItem()
	{
		return CurrentSelection.Pin();
	}
private:

	//Function to set the newly selected item
	void OnSelectionChangedInternal( TSharedPtr<int32> NewSelection, ESelectInfo::Type SelectInfo );

	//Get string to display
	FString OnGetVisibleTextInternal() const
	{
		return VisibleText.Get();
	}

	// Function to create each row of the combo widget
	TSharedRef<ITableRow> OnGenerateComboWidget( TSharedPtr<int32> InComboIndex, const TSharedRef<STableViewBase>& OwnerTable );

	FString GetRowString(int32 RowIndex) const
	{
		return OnGetDisplayName.Execute(RowIndex);
	}

private:
	/** List of items in our combo box. Only generated once as combo items dont change at runtime */
	TArray< TSharedPtr<int32> > ComboItemList;
	
	TAttribute< FString > VisibleText;
	TSharedPtr< SComboButton > ComboButton;
	TSharedPtr< SComboList > ComboList;
	FOnSelectionChanged OnSelectionChanged;
	TWeakPtr<int32> CurrentSelection;

	FGetComboItemDisplayString OnGetDisplayName;
};
