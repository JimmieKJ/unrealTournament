// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the device browser filter bar widget.
 */
class SDeviceBrowserFilterBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceBrowserFilterBar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Default constructor.
	 */
	SDeviceBrowserFilterBar()
		: Filter(NULL)
	{ }

	/**
	 * Destructor.
	 */
	~SDeviceBrowserFilterBar();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InFilter The filter model to use.
	 */
	void Construct(const FArguments& InArgs, FDeviceBrowserFilterRef InFilter);

private:

	// Callback for filter model resets.
	void HandleFilterReset();

	// Callback for changing the filter string text box text.
	void HandleFilterStringTextChanged(const FText& NewText);

	// Callback for changing the checked state of the given platform filter row.
	void HandlePlatformListRowCheckStateChanged(ECheckBoxState CheckState, TSharedPtr<FDeviceBrowserFilterEntry> PlatformEntry);

	// Callback for getting the checked state of the given platform filter row.
	ECheckBoxState HandlePlatformListRowIsChecked(TSharedPtr<FDeviceBrowserFilterEntry> PlatformEntry) const;

	// Callback for getting the text for a row in the platform filter drop-down.
	FText HandlePlatformListRowText(TSharedPtr<FDeviceBrowserFilterEntry> PlatformEntry) const;

	// Generates a row widget for the platform filter list.
	TSharedRef<ITableRow> HandlePlatformListViewGenerateRow(TSharedPtr<FDeviceBrowserFilterEntry> PlatformEntry, const TSharedRef<STableViewBase>& OwnerTable);

private:

	// Holds a pointer to the filter model.
	FDeviceBrowserFilterPtr Filter;

	// Holds the filter string text box.
	TSharedPtr<SSearchBox> FilterStringTextBox;

	// Holds the platform filters list view.
	TSharedPtr<SListView<TSharedPtr<FDeviceBrowserFilterEntry>>> PlatformListView;
};
