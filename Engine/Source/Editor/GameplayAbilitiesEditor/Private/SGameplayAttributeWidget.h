// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttributeSet.h"
#include "SlateBasics.h"

class SGameplayAttributeWidget : public SCompoundWidget
{
public:

	/** Called when a tag status is changed */
	DECLARE_DELEGATE_OneParam(FOnAttributeChanged, UProperty*)

	SLATE_BEGIN_ARGS(SGameplayAttributeWidget)
	: _FilterMetaData()
	, _DefaultProperty(nullptr)
	{}
	SLATE_ARGUMENT(FString, FilterMetaData)
	SLATE_ARGUMENT(UProperty*, DefaultProperty)
	SLATE_EVENT(FOnAttributeChanged, OnAttributeChanged) // Called when a tag status changes
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);	

private:

	TSharedRef<SWidget> GenerateAttributePicker();

	FText GetSelectedValueAsString() const;

	/** Handles updates when the selected attribute changes */
	void OnAttributePicked(UProperty* InProperty);

	/** Delegate to call when the selected attribute changes */
	FOnAttributeChanged OnAttributeChanged;

	/** The search string being used to filter the attributes */
	FString FilterMetaData;

	/** The currently selected attribute */
	UProperty* SelectedProperty;

	/** Used to display an attribute picker. */
	TSharedPtr<class SComboButton> ComboButton;
};
