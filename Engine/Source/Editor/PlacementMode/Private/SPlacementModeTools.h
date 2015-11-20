// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClassIconFinder.h"
#include "AssetData.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"

/**
 * A tile representation of the class or the asset.  These are embedded into the views inside
 * of each tab.
 */
class SPlacementAssetEntry : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SPlacementAssetEntry)
		: _LabelOverride()
		, _AlwaysUseGenericThumbnail(false)
	{}
	
	/** Highlight this text in the text block */
	SLATE_ATTRIBUTE(FText, HighlightText)

	SLATE_ARGUMENT(FText, LabelOverride)

	SLATE_ARGUMENT(FName, ClassThumbnailBrushOverride)

	SLATE_ARGUMENT(bool, AlwaysUseGenericThumbnail)

	SLATE_ARGUMENT(TOptional<FLinearColor>, AssetTypeColorOverride)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UActorFactory* InFactory, const FAssetData& InAsset);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	bool IsPressed() const;

	FText AssetDisplayName;

	UActorFactory* FactoryToUse;
	FAssetData AssetData;

private:
	const FSlateBrush* GetBorder() const;

	bool bIsPressed;

	/** Brush resource that represents a button */
	const FSlateBrush* NormalImage;
	/** Brush resource that represents a button when it is hovered */
	const FSlateBrush* HoverImage;
	/** Brush resource that represents a button when it is pressed */
	const FSlateBrush* PressedImage;
};

class SPlacementModeTools : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPlacementModeTools ){}
	SLATE_END_ARGS();

	void Construct( const FArguments& InArgs );

	virtual ~SPlacementModeTools();

private:

	// Gets the border image for the tab, this is the 'active' orange bar.
	const FSlateBrush* PlacementGroupBorderImage( int32 PlacementGroupIndex ) const;

	// When the tab is clicked we adjust the check state, so that the right style is displayed.
	void OnPlacementTabChanged( ECheckBoxState NewState, int32 PlacementGroupIndex );

	// Gets the tab 'active' state, so that we can show the active style
	ECheckBoxState GetPlacementTabCheckedState( int32 PlacementGroupIndex ) const;

	// Create the standard panel displayed when no search is being performed.
	TSharedRef< SWidget > CreateStandardPanel();

	// Creates a tab widget to show on the left that when clicked actives the right widget switcher index.
	TSharedRef< SWidget > CreatePlacementGroupTab( int32 TabIndex, FText TabText, bool Important );

	// Builds the lights collection widget.
	TSharedRef< SWidget > BuildLightsWidget();

	// Builds the visual collection widget.
	TSharedRef< SWidget > BuildVisualWidget();

	// Builds the 'Basic' collection widget.
	TSharedRef< SWidget > BuildBasicWidget();

	// Called when the recently placed assets changes.
	void UpdateRecentlyPlacedAssets( const TArray< FActorPlacementInfo >& RecentlyPlaced );

	// Rebuilds the recently placed widget
	void RefreshRecentlyPlaced();

	// Rebuilds the 'Classes' widget.
	void RefreshPlaceables();

	// Rebuilds the 'Volumes' widget.
	void RefreshVolumes();

	// Begin SWidget
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	// End SWidget

private:
	void OnSearchChanged(const FText& InFilterText);

	FText GetHighlightText() const;

	// Get the selected panel (normal or search results)
	int32 GetSelectedPanel() const;

	void RebuildPlaceableClassWidgetCache();

	// Flag set when the placeable widget needs to be rebuilt.
	bool bPlaceablesRefreshRequested;

	// Flag set when the placeable widget needs to be rebuilt including the placeable widget cache.
	bool bPlaceablesFullRefreshRequested;

	// Flag set when the volumes widget needs to be rebuilt.
	bool bVolumesRefreshRequested;

	// The text filter used to filter the classes
	typedef TTextFilter<TSharedRef<SPlacementAssetEntry>> FPlacementAssetEntryTextFilter;
	TSharedPtr<FPlacementAssetEntryTextFilter> SearchTextFilter;

	// The search box used to update the filter text
	TSharedPtr<SSearchBox> SearchBoxPtr;

	// The list of placeable classes in widget form, these get put in the search results or
	// the all classes view depending on which is currently visible.
	TArray< TSharedRef<SPlacementAssetEntry> > PlaceableClassWidgets;

	// Holds the widget switcher.
	TSharedPtr<SWidgetSwitcher> WidgetSwitcher;

	// The container widget for the recently placed.
	TSharedPtr< SVerticalBox > RecentlyPlacedContainer;

	// The container widget for the volumes.
	TSharedPtr< SVerticalBox > VolumesContainer;

	// The container widget for the classes.
	TSharedPtr< SVerticalBox > PlaceablesContainer;

	// The container widget for the classes search results.
	TSharedPtr< SVerticalBox > SearchResultsContainer;
};
