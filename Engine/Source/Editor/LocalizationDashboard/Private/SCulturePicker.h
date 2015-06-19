// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"
#include "SlateDelegates.h"
#include "CulturePointer.h"

struct FCultureEntry
{
	FCulturePtr Culture;
	TArray< TSharedPtr<FCultureEntry> > Children;
	bool IsSelectable;

	FCultureEntry(const FCulturePtr& InCulture, const bool InIsSelectable = true)
		: Culture(InCulture)
		, IsSelectable(InIsSelectable)
	{}

	FCultureEntry(const FCultureEntry& Source)
		: Culture(Source.Culture)
		, IsSelectable(Source.IsSelectable)
	{
		Children.Reserve(Source.Children.Num());
		for (const auto& Child : Source.Children)
		{
			Children.Add( MakeShareable( new FCultureEntry(*Child) ) );
		}
	}
};

class SCulturePicker : public SCompoundWidget
{
public:
	/** A delegate type invoked when a selection changes somewhere. */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsCulturePickable, FCulturePtr);
	typedef TSlateDelegates< FCulturePtr >::FOnSelectionChanged FOnSelectionChanged;

public:
	SLATE_BEGIN_ARGS( SCulturePicker ){}
		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )
		SLATE_EVENT( FIsCulturePickable, IsCulturePickable )
		SLATE_ARGUMENT( FCulturePtr, InitialSelection )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
	void RequestTreeRefresh();

private:
	void BuildStockEntries();
	void RebuildEntries();

	void OnFilterStringChanged(const FText& InFilterString);
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FCultureEntry> Entry, const TSharedRef<STableViewBase>& Table);
	void OnGetChildren(TSharedPtr<FCultureEntry> Entry, TArray< TSharedPtr<FCultureEntry> >& Children);
	void OnSelectionChanged(TSharedPtr<FCultureEntry> Entry, ESelectInfo::Type SelectInfo);

private:
	TSharedPtr< STreeView< TSharedPtr<FCultureEntry> > > TreeView;

	/* The provided cultures array. */
	TArray<FCulturePtr> Cultures;

	/* The top level culture entries for all possible stock cultures. */
	TArray< TSharedPtr<FCultureEntry> > StockEntries;

	/* The top level culture entries to be displayed in the tree view. */
	TArray< TSharedPtr<FCultureEntry> > RootEntries;

	/* The string by which to filter cultures names. */
	FString FilterString;

	/** Delegate to invoke when selection changes. */
	FOnSelectionChanged OnCultureSelectionChanged;

	/** Delegate to invoke to set whether a culture is "pickable". */
	FIsCulturePickable IsCulturePickable;
};