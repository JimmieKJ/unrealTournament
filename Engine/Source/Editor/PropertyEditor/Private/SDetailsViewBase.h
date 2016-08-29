// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailsViewPrivate.h"
#include "AssetSelection.h"
#include "IPropertyUtilities.h"


class FDetailCategoryImpl;
class FDetailLayoutBuilderImpl;


struct FPropertyNodeMap
{
	FPropertyNodeMap()
		: ParentProperty(NULL)
	{}

	/** Object property node which contains the properties in the node map */
	FPropertyNode* ParentProperty;

	/** Property name to property node map */
	TMap<FName, TSharedPtr<FPropertyNode> > PropertyNameToNode;

	bool Contains(FName PropertyName) const
	{
		return PropertyNameToNode.Contains(PropertyName);
	}

	void Add(FName PropertyName, TSharedPtr<FPropertyNode>& PropertyNode)
	{
		PropertyNameToNode.Add(PropertyName, PropertyNode);
	}
};

typedef TArray< TSharedRef<class IDetailTreeNode> > FDetailNodeList;

/** Mapping of categories to all top level item property nodes in that category */
typedef TMap<FName, TSharedPtr<FDetailCategoryImpl> > FCategoryMap;

/** Class to properties in that class */
typedef TMap<FName, FPropertyNodeMap> FClassInstanceToPropertyMap;

/** Class to properties in that class */
typedef TMap<FName, FClassInstanceToPropertyMap> FClassToPropertyMap;

typedef STreeView< TSharedRef<class IDetailTreeNode> > SDetailTree;

typedef TArray<TSharedPtr<FComplexPropertyNode>> FRootPropertyNodeList;

struct FDetailLayoutData
{
	TSharedPtr<FDetailLayoutBuilderImpl> DetailLayout;

	FClassToPropertyMap ClassToPropertyMap;

	/** A  unique classes being viewed */
	TSet< TWeakObjectPtr<UStruct> > ClassesWithProperties;

	/** Customization class instances currently active in this view */
	TArray< TSharedPtr<IDetailCustomization> > CustomizationClassInstances;

};

typedef TArray<FDetailLayoutData> FDetailLayoutList;

/** Represents a filter which controls the visibility of items in the details view */
struct FDetailFilter
{
	FDetailFilter()
		: bShowOnlyModifiedProperties(false)
		, bShowAllAdvanced(false)
		, bShowOnlyDiffering(false)
		, bShowAllChildrenIfCategoryMatches(true)
	{}

	bool IsEmptyFilter() const { return FilterStrings.Num() == 0 && bShowOnlyModifiedProperties == false && bShowAllAdvanced == false && bShowOnlyDiffering == false && bShowAllChildrenIfCategoryMatches == false; }

	/** Any user search terms that items must match */
	TArray<FString> FilterStrings;
	/** If we should only show modified properties */
	bool bShowOnlyModifiedProperties;
	/** If we should show all advanced properties */
	bool bShowAllAdvanced;
	/** If we should only show differing properties */
	bool bShowOnlyDiffering;
	/** If we should show all the children if their category name matches the search */
	bool bShowAllChildrenIfCategoryMatches;
	TSet<FPropertyPath> WhitelistedProperties;
};

struct FDetailColumnSizeData
{
	TAttribute<float> LeftColumnWidth;
	TAttribute<float> RightColumnWidth;
	SSplitter::FOnSlotResized OnWidthChanged;

	void SetColumnWidth(float InWidth) { OnWidthChanged.ExecuteIfBound(InWidth); }
};

class SDetailsViewBase : public IDetailsViewPrivate
{
public:
	SDetailsViewBase()
		: ColumnWidth(.65f)
		, bHasActiveFilter(false)
		, bIsLocked(false)
		, bHasOpenColorPicker(false)
		, bDisableCustomDetailLayouts( false )
		, NumVisbleTopLevelObjectNodes(0)
	{
	}

	virtual ~SDetailsViewBase();

	/**
	* @return true of the details view can be updated from editor selection
	*/
	virtual bool IsUpdatable() const override
	{
		return DetailsViewArgs.bUpdatesFromSelection;
	}

	virtual bool HasActiveSearch() const override
	{
		return CurrentFilter.FilterStrings.Num() > 0;
	}

	virtual int32 GetNumVisibleTopLevelObjects() const override
	{
		return NumVisbleTopLevelObjectNodes;
	}


	/** @return The identifier for this details view, or NAME_None is this view is anonymous */
	virtual FName GetIdentifier() const override
	{
		return DetailsViewArgs.ViewIdentifier;
	}

	/**
	 * Sets the visible state of the filter box/property grid area
	 */
	virtual void HideFilterArea(bool bHide) override;

	/** 
	 * IDetailsView interface 
	 */
	virtual TArray< FPropertyPath > GetPropertiesInOrderDisplayed() const override;
	virtual void HighlightProperty(const FPropertyPath& Property) override;
	virtual void ShowAllAdvancedProperties() override;
	virtual void SetOnDisplayedPropertiesChanged(FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChangedDelegate) override;
	virtual void SetDisableCustomDetailLayouts( bool bInDisableCustomDetailLayouts ) override { bDisableCustomDetailLayouts = bInDisableCustomDetailLayouts; }
	virtual void SetIsPropertyVisibleDelegate(FIsPropertyVisible InIsPropertyVisible) override;
	virtual void SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly InIsPropertyReadOnly) override;
	virtual void SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled IsPropertyEditingEnabled) override;
	virtual bool IsPropertyEditingEnabled() const override;
	virtual void SetKeyframeHandler(TSharedPtr<class IDetailKeyframeHandler> InKeyframeHandler) override;
	virtual TSharedPtr<IDetailKeyframeHandler> GetKeyframeHandler() override;
	virtual void SetExtensionHandler(TSharedPtr<class IDetailPropertyExtensionHandler> InExtensionHandler) override;
	virtual TSharedPtr<IDetailPropertyExtensionHandler> GetExtensionHandler() override;
	virtual bool IsPropertyVisible(const struct FPropertyAndParent& PropertyAndParent) const override;
	virtual bool IsPropertyReadOnly(const struct FPropertyAndParent& PropertyAndParent) const override;
	virtual void SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance OnGetGenericDetails) override;
	virtual bool IsLocked() const override { return bIsLocked; }
	virtual void RefreshRootObjectVisibility() override;
	virtual FOnFinishedChangingProperties& OnFinishedChangingProperties() override { return OnFinishedChangingPropertiesDelegate; }

	/** IDetailsViewPrivate interface */
	virtual void RerunCurrentFilter() override;
	void SetNodeExpansionState(TSharedRef<IDetailTreeNode> InTreeNode, bool bIsItemExpanded, bool bRecursive) override;
	void SaveCustomExpansionState(const FString& NodePath, bool bIsExpanded) override;
	bool GetCustomSavedExpansionState(const FString& NodePath) const override;
	virtual void NotifyFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent) override;
	void RefreshTree() override;
	TSharedPtr<class FAssetThumbnailPool> GetThumbnailPool() const override;
	TSharedPtr<IPropertyUtilities> GetPropertyUtilities() override;
	void CreateColorPickerWindow(const TSharedRef< class FPropertyEditor >& PropertyEditor, bool bUseAlpha) override;
	virtual FNotifyHook* GetNotifyHook() const override { return DetailsViewArgs.NotifyHook; }

	virtual bool IsConnected() const = 0;

	virtual FRootPropertyNodeList& GetRootNodes() = 0;

	/**
	 * Called when the open color picker window associated with this details view is closed
	 */
	void OnColorPickerWindowClosed(const TSharedRef<SWindow>& Window);

	/**
	 * Requests that an item in the tree be expanded or collapsed
	 *
	 * @param TreeNode	The tree node to expand
	 * @param bExpand	true if the item should be expanded, false otherwise
	 */
	void RequestItemExpanded(TSharedRef<IDetailTreeNode> TreeNode, bool bExpand) override;

	/**
	 * Sets the expansion state all root nodes and optionally all of their children
	 *
	 * @param bExpand			The new expansion state
	 * @param bRecurse			Whether or not to apply the expansion change to any children
	 */
	void SetRootExpansionStates(const bool bExpand, const bool bRecurse);

	/**
	 * Queries a layout for a specific class
	 */
	void QueryLayoutForClass(FDetailLayoutData& LayoutData, UStruct* Class);

	/**
	 * Calls a delegate for each registered class that has properties visible to get any custom detail layouts
	 */
	void QueryCustomDetailLayout(FDetailLayoutData& LayoutData);

	/** Column width accessibility */
	float OnGetLeftColumnWidth() const { return 1.0f - ColumnWidth; }
	float OnGetRightColumnWidth() const { return ColumnWidth; }
	void OnSetColumnWidth(float InWidth) { ColumnWidth = InWidth; }

	/**
	 * Adds an action to execute next tick
	 */
	virtual void EnqueueDeferredAction(FSimpleDelegate& DeferredAction) override;


	// SWidget interface
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

	/** Saves the expansion state of property nodes for the selected object set */
	void SaveExpandedItems( TSharedRef<FPropertyNode> StartNode );

	/**
	* Restores the expansion state of property nodes for the selected object set
	*/
	void RestoreExpandedItems(TSharedRef<FPropertyNode> StartNode);


protected:
	/**
	 * Called when a color property is changed from a color picker
	 */
	void SetColorPropertyFromColorPicker(FLinearColor NewColor);

	/** Updates the property map for access when customizing the details view.  Generates default layout for properties */
	void UpdatePropertyMaps();
	void UpdateSinglePropertyMap(TSharedPtr<FComplexPropertyNode>& InRootPropertyNode, FDetailLayoutData& LayoutData);

	/** 
	 * Recursively updates children of property nodes. Generates default layout for properties 
	 * 
	 * @param InNode	The parent node to get children from
	 * @param The detail layout builder that will be used for customization of this property map
	 * @param CurCategory The current category name
	 */
	void UpdateSinglePropertyMapRecursive( FPropertyNode& InNode, FDetailLayoutData& LayoutData, FName CurCategory, FComplexPropertyNode* CurObjectNode, bool bEnableFavoriteSystem, bool bUpdateFavoriteSystemOnly);

	virtual void CustomUpdatePropertyMap(TSharedPtr<FDetailLayoutBuilderImpl>& InDetailLayout) {}

	/** Called to get the visibility of the tree view */
	EVisibility GetTreeVisibility() const;

	/** Returns the name of the image used for the icon on the filter button */
	const FSlateBrush* OnGetFilterButtonImageResource() const;

	/** Called when the locked button is clicked */
	FReply OnLockButtonClicked();

	/**
	 * Called to recursively expand/collapse the children of the given item
	 *
	 * @param InTreeNode		The node that was expanded or collapsed
	 * @param bIsItemExpanded	True if the item is expanded, false if it is collapsed
	 */
	void SetNodeExpansionStateRecursive( TSharedRef<IDetailTreeNode> InTreeNode, bool bIsItemExpanded );

	/**
	 * Called when an item is expanded or collapsed in the detail tree
	 *
	 * @param InTreeNode		The node that was expanded or collapsed
	 * @param bIsItemExpanded	True if the item is expanded, false if it is collapsed
	 */
	void OnItemExpansionChanged( TSharedRef<IDetailTreeNode> InTreeNode, bool bIsItemExpanded );

	/** 
	 * Function called through a delegate on the TreeView to request children of a tree node 
	 * 
	 * @param InTreeNode		The tree node to get children from
	 * @param OutChildren		The list of children of InTreeNode that should be visible 
	 */
	void OnGetChildrenForDetailTree( TSharedRef<IDetailTreeNode> InTreeNode, TArray< TSharedRef<IDetailTreeNode> >& OutChildren );

	/**
	 * Returns an SWidget used as the visual representation of a node in the treeview.                     
	 */
	TSharedRef<ITableRow> OnGenerateRowForDetailTree( TSharedRef<IDetailTreeNode> InTreeNode, const TSharedRef<STableViewBase>& OwnerTable );

	/** @return true if show only modified is checked */
	bool IsShowOnlyModifiedChecked() const { return CurrentFilter.bShowOnlyModifiedProperties; }

	/** @return true if show all advanced is checked */
	bool IsShowAllAdvancedChecked() const { return CurrentFilter.bShowAllAdvanced; }

	/** @return true if show only differing is checked */
	bool IsShowOnlyDifferingChecked() const { return CurrentFilter.bShowOnlyDiffering; }

	/** @return true if show all advanced is checked */
	bool IsShowAllChildrenIfCategoryMatchesChecked() const { return CurrentFilter.bShowAllChildrenIfCategoryMatches; }

	/** Called when show only modified is clicked */
	void OnShowOnlyModifiedClicked();

	/** Called when show all advanced is clicked */
	void OnShowAllAdvancedClicked();

	/** Called when show only differing is clicked */
	void OnShowOnlyDifferingClicked();

	/** Called when show all children if category matches is clicked */
	void OnShowAllChildrenIfCategoryMatchesClicked();

	/**
	* Updates the details with the passed in filter
	*/
	void UpdateFilteredDetails();

	/** Called when the filter text changes.  This filters specific property nodes out of view */
	void OnFilterTextChanged(const FText& InFilterText);

	/** Called when the list of currently differing properties changes */
	virtual void UpdatePropertiesWhitelist(const TSet<FPropertyPath> InWhitelistedProperties) override { CurrentFilter.WhitelistedProperties = InWhitelistedProperties; }

	virtual TSharedPtr<SWidget> GetNameAreaWidget() override;
	virtual TSharedPtr<SWidget> GetFilterAreaWidget() override;
	virtual TSharedPtr<class FUICommandList> GetHostCommandList() const override;

	/** 
	 * Hides or shows properties based on the passed in filter text
	 * 
	 * @param InFilterText	The filter text
	 */
	void FilterView( const FString& InFilterText );

	/** Called to get the visibility of the filter box */
	EVisibility GetFilterBoxVisibility() const;

	/** Utility function allowing derived classes to optionally implement RegisterInstancedCustomPropertyLayout */
	void RegisterInstancedCustomPropertyLayoutInternal(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate);

	/** Utility function allowing derived classes to optionally implement UnregisterInstancedCustomPropertyLayout */
	void UnregisterInstancedCustomPropertyLayoutInternal(UStruct* Class);

protected:
	/** The user defined args for the details view */
	FDetailsViewArgs DetailsViewArgs;
	/** A mapping of classes to detail layout delegates, called when querying for custom detail layouts in this instance of the details view only*/
	FCustomDetailLayoutMap InstancedClassToDetailLayoutMap;
	/** The current detail layout based on objects in this details panel.  There is one layout for each top level object node.*/
	FDetailLayoutList DetailLayouts;
	/** Row for searching and view options */
	TSharedPtr<SHorizontalBox>  FilterRow;
	/** Search box */
	TSharedPtr<SSearchBox> SearchBox;
	/** Customization instances that need to be destroyed when safe to do so */
	TArray< TSharedPtr<IDetailCustomization> > CustomizationClassInstancesPendingDelete;
	/** Map of nodes that are requesting an automatic expansion/collapse due to being filtered */
	TMap< TSharedRef<IDetailTreeNode>, bool > FilteredNodesRequestingExpansionState;
	/** Current set of expanded detail nodes (by path) that should be saved when the details panel closes */
	TSet<FString> ExpandedDetailNodes;
	/** Tree view */
	TSharedPtr<SDetailTree> DetailTree;
	/** Root tree nodes visible in the tree */
	FDetailNodeList RootTreeNodes;
	/** Delegate executed to determine if a property should be visible */
	FIsPropertyVisible IsPropertyVisibleDelegate;
	/** Delegate executed to determine if a property should be read-only */
	FIsPropertyReadOnly IsPropertyReadOnlyDelegate;
	/** Delegate called to see if a property editing is enabled */
	FIsPropertyEditingEnabled IsPropertyEditingEnabledDelegate;
	/** Delegate called when the details panel finishes editing a property (after post edit change is called) */
	FOnFinishedChangingProperties OnFinishedChangingPropertiesDelegate;
	/** Container for passing around column size data to rows in the tree (each row has a splitter which can affect the column size)*/
	FDetailColumnSizeData ColumnSizeData;
	/** The actual width of the right column.  The left column is 1-ColumnWidth */
	float ColumnWidth;
	/** True if there is an active filter (text in the filter box) */
	bool bHasActiveFilter;
	/** True if this property view is currently locked (I.E The objects being observed are not changed automatically due to user selection)*/
	bool bIsLocked;
	/** The property node that the color picker is currently editing. */
	TWeakPtr<FPropertyNode> ColorPropertyNode;
	/** Whether or not this instance of the details view opened a color picker and it is not closed yet */
	bool bHasOpenColorPicker;
	/** Settings for this view */
	TSharedPtr<class IPropertyUtilities> PropertyUtilities;
	/** The name area which is not recreated when selection changes */
	TSharedPtr<class SDetailNameArea> NameArea;
	/** Asset pool for rendering and managing asset thumbnails visible in this view*/
	mutable TSharedPtr<class FAssetThumbnailPool> ThumbnailPool;
	/** The current filter */
	FDetailFilter CurrentFilter;
	/** Delegate called to get generic details not specific to an object being viewed */
	FOnGetDetailCustomizationInstance GenericLayoutDelegate;
	/** Actions that should be executed next tick */
	TArray<FSimpleDelegate> DeferredActions;

	/** Root tree nodes that needs to be destroyed when safe */
	FRootPropertyNodeList RootNodesPendingKill;

	/** The handler for the keyframe UI, determines if the key framing UI should be displayed. */
	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler;

	/** Property extension handler returns additional UI to apply after the customization is applied to the property. */
	TSharedPtr<IDetailPropertyExtensionHandler> ExtensionHandler;

	/** External property nodes which need to validated each tick */
	TArray< TWeakPtr<FPropertyNode> > ExternalRootPropertyNodes;

	/** The tree node that is currently highlighted, may be none: */
	TWeakPtr< IDetailTreeNode > CurrentlyHighlightedNode;

	/** Executed when the tree is refreshed */
	FOnDisplayedPropertiesChanged OnDisplayedPropertiesChangedDelegate;

	/** True if we want to skip generation of custom layouts for displayed object */
	bool bDisableCustomDetailLayouts;

	int32 NumVisbleTopLevelObjectNodes;
};
