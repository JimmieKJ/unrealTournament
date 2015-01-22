// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* A list of filters currently applied to an asset view.
*/
struct FSimpleGraphFilter
{
	FSimpleGraphFilter(FName InName) : Name(InName), bEnabled(true) {}
	FSimpleGraphFilter(FName InName, bool InEnabled) : Name(InName), bEnabled(InEnabled) {}

	FName Name;
	uint32 bEnabled : 1;
};

FORCEINLINE bool operator==(const FSimpleGraphFilter& Left, const FSimpleGraphFilter& Right)
{
	return Left.Name == Right.Name;
}


class SFilterWidget;
class SVisualLoggerFilters : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVisualLoggerFilters) { }
	SLATE_END_ARGS()
		
	void Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList, TSharedPtr<IVisualLoggerInterface> VisualLoggerInterface);
	void AddFilter(const FString& InFilterName);
	void AddFilter(const FString& GraphName, const FString& DataName);
	FLinearColor GetColorForUsedCategory(int32 Index) const;
	FLinearColor GetColorForUsedCategory(const FString& InFilterName) const;
	uint32 GetCategoryIndex(const FString& InFilterName) const;
	bool IsFilterEnabled(const FString& InFilterName, TEnumAsByte<ELogVerbosity::Type> Verbosity = ELogVerbosity::All);
	bool IsFilterEnabled(const FString& InGraphName, const FString& InDataName, TEnumAsByte<ELogVerbosity::Type> Verbosity = ELogVerbosity::All);
	void OnFiltersSearchChanged(const FText& Filter);
	void OnSearchChanged(const FText& Filter);
	bool GraphSubmenuVisibility(const FName MenuName);
	void FilterByTypeClicked(FName InGraphName, FName InDataName);
	bool IsAssetTypeActionsInUse(FName InGraphName, FName InDataName) const;
	void InvalidateCanvas();
	void GraphFilterCategoryClicked(FName MenuCategory);
	bool IsGraphFilterCategoryInUse(FName MenuCategory) const;

	TSharedRef<SWidget> MakeGraphsFilterMenu();
	void CreateFiltersMenuCategoryForGraph(FMenuBuilder& MenuBuilder, FName MenuCategory) const;
	bool HasFilters() { return Filters.Num() || GraphFilters.Num(); }
protected:
	/** The horizontal box which contains all the filters */
	TSharedPtr<SWrapBox> FilterBox;
	TSharedPtr<IVisualLoggerInterface> VisualLoggerInterface;
	TArray<TSharedRef<SFilterWidget> > Filters;
	TMap<FName, TArray<FSimpleGraphFilter> > GraphFilters;
	FString FiltersSearchString;
	TSharedPtr<SComboButton> GraphsFilterCombo;
	FString GraphsFilter;
	static FColor ColorPalette[];
};
