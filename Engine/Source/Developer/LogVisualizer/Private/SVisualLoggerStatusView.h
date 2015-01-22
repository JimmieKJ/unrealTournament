// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SVisualLoggerStatusView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVisualLoggerStatusView){}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList);

	void OnItemSelectionChanged(const FVisualLogDevice::FVisualLogEntryItem&);

	TSharedRef<ITableRow> HandleGenerateLogStatus(TSharedPtr<struct FLogStatusItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnLogStatusGetChildren(TSharedPtr<struct FLogStatusItem> InItem, TArray< TSharedPtr<FLogStatusItem> >& OutItems);
protected:
	TSharedPtr< STreeView< TSharedPtr<FLogStatusItem> > > StatusItemsView;
	TArray< TSharedPtr<FLogStatusItem> > StatusItems;
};
