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

	void OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);

	FOnAttributeChanged OnAttributeChanged;

	FString FilterMetaData;

	TArray<TSharedPtr<FString>> PropertyOptions;

	UProperty* SelectedProperty;
};
