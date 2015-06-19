// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* A list of filters currently applied to an asset view.
*/
class SFilterWidget;
class SVisualLoggerFilters : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVisualLoggerFilters) { }
	SLATE_END_ARGS()
		
	void Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList);
	void AddFilter(const FString& InFilterName);
	void AddFilter(const FString& GraphName, const FString& DataName);
	uint32 GetCategoryIndex(const FString& InFilterName) const;
	void OnFiltersSearchChanged(const FText& Filter);
	void OnSearchChanged(const FText& Filter);
	bool GraphSubmenuVisibility(const FName MenuName);
	void FilterByTypeClicked(FName InGraphName, FName InDataName);
	bool IsAssetTypeActionsInUse(FName InGraphName, FName InDataName) const;
	void InvalidateCanvas();
	void GraphFilterCategoryClicked(FName MenuCategory);
	bool IsGraphFilterCategoryInUse(FName MenuCategory) const;
	void OnFiltersChanged();
	void ResetData();
	void OnItemSelectionChanged(const struct FVisualLogEntry& EntryItem);

	TSharedRef<SWidget> MakeGraphsFilterMenu();
	void CreateFiltersMenuCategoryForGraph(FMenuBuilder& MenuBuilder, FName MenuCategory) const;
	bool HasFilters() { return Filters.Num() || GraphFilters.Num(); }
protected:
	/** The horizontal box which contains all the filters */
	TSharedPtr<SWrapBox> FilterBox;
	TArray<TSharedRef<SFilterWidget> > Filters;
	TMap<FName, TArray<FString> > GraphFilters;
	TSharedPtr<SComboButton> GraphsFilterCombo;
	FString GraphsFilter;
};
