// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SVisualLoggerLogsList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVisualLoggerLogsList){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList);

	TSharedRef<ITableRow> LogEntryLinesGenerateRow(TSharedPtr<struct FLogEntryItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void LogEntryLineSelectionChanged(TSharedPtr<FLogEntryItem> SelectedItem, ESelectInfo::Type SelectInfo);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void OnItemSelectionChanged(const FVisualLogDevice::FVisualLogEntryItem& EntryItem);
	void OnFiltersChanged();
	void OnFiltersSearchChanged(const FText& Filter);
	FText GetFilterText() const;
	const FVisualLogDevice::FVisualLogEntryItem& GetCurrentLogEntry() { return CurrentLogEntry; }

protected:
	TSharedPtr<SListView<TSharedPtr<struct FLogEntryItem> > > LogsLinesWidget;
	TArray<TSharedPtr<struct FLogEntryItem> > LogEntryLines;
	FVisualLogDevice::FVisualLogEntryItem CurrentLogEntry;
};
