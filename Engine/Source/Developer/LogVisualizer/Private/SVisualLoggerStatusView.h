// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SVisualLoggerStatusView : public SVisualLoggerBaseWidget
{
public:
	SLATE_BEGIN_ARGS(SVisualLoggerStatusView){}
	SLATE_END_ARGS();

	virtual ~SVisualLoggerStatusView();
	void Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList);
	void ResetData();

protected:
	void OnObjectSelectionChanged(const TArray<FName>& SelectedRows);
	void OnItemSelectionChanged(const FVisualLoggerDBRow& DBRow, int32 ItemIndex);
	void GenerateStatusData(const FVisualLogDevice::FVisualLogEntryItem&, bool bAddHeader);

	TSharedRef<ITableRow> HandleGenerateLogStatus(TSharedPtr<struct FLogStatusItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnLogStatusGetChildren(TSharedPtr<struct FLogStatusItem> InItem, TArray< TSharedPtr<FLogStatusItem> >& OutItems);
	void OnExpansionChanged(TSharedPtr<FLogStatusItem> Item, bool);

protected:
	TArray<FString> ExpandedCategories;
	TSharedPtr< STreeView< TSharedPtr<FLogStatusItem> > > StatusItemsView;
	TArray< TSharedPtr<FLogStatusItem> > StatusItems;
};
