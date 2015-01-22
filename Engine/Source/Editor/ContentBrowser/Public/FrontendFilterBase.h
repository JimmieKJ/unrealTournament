// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

typedef const FAssetData& AssetFilterType;
typedef TFilterCollection<AssetFilterType> AssetFilterCollectionType;

class FFrontendFilterCategory
{
public:
	FFrontendFilterCategory(const FText& InTitle, const FText& InTooltip) : Title(InTitle), Tooltip(InTooltip) {}

	/** The title of this category, used for the menu heading */
	const FText Title;

	/** The menu tooltip for this category */
	const FText Tooltip;
};

class FFrontendFilter : public IFilter<AssetFilterType>
{
public:
	FFrontendFilter(TSharedPtr<FFrontendFilterCategory> InCategory) : FilterCategory(InCategory) {}

	/** Returns the system name for this filter */
	virtual FString GetName() const = 0;

	/** Returns the human readable name for this filter */
	virtual FText GetDisplayName() const = 0;

	/** Returns the tooltip for this filter, shown in the filters menu */
	virtual FText GetToolTipText() const = 0;

	/** Returns the color this filter button will be when displayed as a button */
	virtual FLinearColor GetColor() const { return FLinearColor(0.6f, 0.6f, 0.6f, 1); }

	/** Returns the name of the icon to use in menu entries */
	virtual FName GetIconName() const { return NAME_None; }

	/** Returns true if the filter should be in the list when disabled and not in the list when enabled */
	virtual bool IsInverseFilter() const { return false; }

	/** Invoke to set the ARFilter that is currently used to filter assets in the asset view */
	virtual void SetCurrentFilter(const FARFilter& InBaseFilter) { }

	/** Notification that the filter became active or inactive */
	virtual void ActiveStateChanged(bool bActive) { }

	// IFilter implementation
	DECLARE_DERIVED_EVENT( FFrontendFilter, IFilter<AssetFilterType>::FChangedEvent, FChangedEvent );
	virtual FChangedEvent& OnChanged() override { return ChangedEvent; }

	TSharedPtr<FFrontendFilterCategory> GetCategory() { return FilterCategory; }

protected:
	void BroadcastChangedEvent() const { ChangedEvent.Broadcast(); }

private:
	FChangedEvent ChangedEvent;

	TSharedPtr<FFrontendFilterCategory> FilterCategory;
};