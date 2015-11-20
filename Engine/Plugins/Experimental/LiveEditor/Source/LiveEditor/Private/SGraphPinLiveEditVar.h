// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"
#include "SGraphPinComboBox.h"

class SGraphPinLiveEditVar : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinLiveEditVar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, UClass *LiveEditObjectClass);

protected:

	/**
	 *	Function to create class specific widget.
	 *
	 *	@return Reference to the newly created widget object
	 */
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

private:

	/**
	 *	Function to get current string associated with the combo box selection
	 *
	 *	@return currently selected string
	 */
	FString OnGetText() const;

	/**
	 *	Function to generate the list of indexes from the enum object
	 *
	 *	@param OutComboBoxIndexes - Int array reference to store the list of indexes
	 */
	void GenerateComboBoxIndexes( TArray< TSharedPtr<int32> >& OutComboBoxIndexes );
	void GenerateComboBoxIndexesRecurse( UStruct *InStruct, FString PropertyPrefix, TArray< TSharedPtr<int32> >& OutComboBoxIndexes );

	/**
	 *	Function to set the newly selected index
	 *
	 * @param NewSelection The newly selected item in the combo box
	 * @param SelectInfo Provides context on how the selection changed
	 */
	void ComboBoxSelectionChanged( TSharedPtr<int32> NewSelection, ESelectInfo::Type SelectInfo );

	/**
	 * Returns the friendly name of the enum at index EnumIndex
	 *
	 * @param EnumIndex	- The index of the enum to return the friendly name for
	 */
	FText OnGetFriendlyName(int32 EnumIndex) const;

	/**
	 * Executes a series of checks to determine if this is a variable that the LiveEditor should really have access to
	 **/
	bool IsPropertyPermitedForLiveEditor( UProperty &Property );

private:
	TSharedPtr<class SPinComboBox>	ComboBox;
	TWeakObjectPtr<class UObject> NodeClass;
	TArray<FString> VariableNameList;
	FString CurrentValue;
};
