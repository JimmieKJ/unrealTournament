// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Implements a row widget for query list. */
class SCAQueryTableRow : public SMultiColumnTableRow< TSharedPtr<class FQueryTreeItem> >
{
public:

	SLATE_BEGIN_ARGS(SCAQueryTableRow) {}
		SLATE_ARGUMENT(TSharedPtr<class SCollisionAnalyzer>, OwnerAnalyzerWidget)
		SLATE_ARGUMENT(TSharedPtr<class FQueryTreeItem>, Item)
	SLATE_END_ARGS()


public:

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	// delegate
	FText GetTotalTimeText() const;

private:

	/** Tree item */
	TSharedPtr<class FQueryTreeItem>	Item;
	/** Analyzer widget that owns us */
	TWeakPtr<SCollisionAnalyzer>		OwnerAnalyzerWidgetPtr;
};