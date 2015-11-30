// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSequencer;
class FSequencerNodeTree;
class FSequencerDisplayNode;

class SSequencerTrackLane;
class SSequencerTrackArea;
class SSequencerTreeViewRow;

typedef TSharedRef<FSequencerDisplayNode> FDisplayNodeRef;

enum class ETreeRecursion
{
	Recursive, NonRecursive
};

/** Structure used to define a column in the tree view */
struct FSequencerTreeViewColumn
{
	typedef TFunction<TSharedRef<SWidget>(const FDisplayNodeRef&, const TSharedRef<SSequencerTreeViewRow>&)> FOnGenerate;

	FSequencerTreeViewColumn(const FOnGenerate& InOnGenerate, const TAttribute<float>& InWidth) : Generator(InOnGenerate), Width(InWidth) {}
	FSequencerTreeViewColumn(FOnGenerate&& InOnGenerate, const TAttribute<float>& InWidth) : Generator(MoveTemp(InOnGenerate)), Width(InWidth) {}

	/** Function used to generate a cell for this column */
	FOnGenerate Generator;
	/** Attribute specifying the width of this column */
	TAttribute<float> Width;
};

/** The tree view used in the sequencer */
class SSequencerTreeView : public STreeView<FDisplayNodeRef>
{
public:

	SLATE_BEGIN_ARGS(SSequencerTreeView){}
		/** Externally supplied scroll bar */
		SLATE_ARGUMENT( TSharedPtr<SScrollBar>, ExternalScrollbar )
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<FSequencerNodeTree>& InNodeTree, const TSharedRef<SSequencerTrackArea>& InTrackArea);
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	/** Access the underlying tree data */
	TSharedPtr<FSequencerNodeTree> GetNodeTree() { return SequencerNodeTree; }

public:

	/** Get the display node at the specified physical vertical position */
	TSharedPtr<FSequencerDisplayNode> HitTestNode(float InPhysical) const;

	/** Convert the specified physical vertical position into an absolute virtual position, ignoring expanded states */
	float PhysicalToVirtual(float InPhysical) const;

	/** Convert the specified absolute virtual position into a physical position in the tree.
	 * @note: Will not work reliably for virtual positions that are outside of the physical space
	 */
	float VirtualToPhysical(float InVirtual) const;

public:

	/** Refresh this tree as a result of the underlying tree data changing */
	void Refresh();

	/** Expand or collapse nodes */
	void ToggleExpandCollapseNodes(ETreeRecursion Recursion = ETreeRecursion::Recursive, bool bExpandAll = false);

	/** Expand nodes */
	void ExpandNodes(ETreeRecursion Recursion = ETreeRecursion::Recursive, bool bExpandAll = false);

	/** Collapse nodes */
	void CollapseNodes(ETreeRecursion Recursion = ETreeRecursion::Recursive, bool bExpandAll = false);

	/** Scroll this tree view by the specified number of slate units */
	void ScrollByDelta(float DeltaInSlateUnits);

protected:

	/** Expand or collapse nodes */
	void ExpandOrCollapseNodes(ETreeRecursion Recursion, bool bExpandAll, bool bExpand);

	/** Set the item's expansion state, including all of its children */
	void ExpandCollapseNode(const FDisplayNodeRef& InNode, bool bExpansionState, ETreeRecursion Recursion);

	/** Generate a row for a particular node */
	TSharedRef<ITableRow> OnGenerateRow(FDisplayNodeRef InDisplayNode, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gather the children from the specified node */
	void OnGetChildren(FDisplayNodeRef InParent, TArray<FDisplayNodeRef>& OutChildren) const;

	/** Generate a widget for the specified Node and Column */
	TSharedRef<SWidget> GenerateWidgetForColumn(const FDisplayNodeRef& Node, const FName& ColumnId, const TSharedRef<SSequencerTreeViewRow>& Row) const;

	/** Called when a node has been expanded or collapsed */
	void OnExpansionChanged(FDisplayNodeRef InItem, bool bIsExpanded);

	/** Called when the sequencer outliner selection has been changed externally */
	void OnSequencerSelectionChangedExternally();

	/** Overridden to keep external selection states up to date */
	virtual void Private_SignalSelectionChanged(ESelectInfo::Type SelectInfo) override;

public:
	
	/** Structure used to cache physical geometry for a particular node */
	struct FCachedGeometry
	{
		FCachedGeometry(FDisplayNodeRef InNode, float InPhysicalTop, float InPhysicalHeight)
			: Node(MoveTemp(InNode)), PhysicalTop(InPhysicalTop), PhysicalHeight(InPhysicalHeight)
		{}

		FDisplayNodeRef Node;
		float PhysicalTop, PhysicalHeight;
	};

	/** Access all the physical nodes currently visible on the sequencer */
	const TArray<FCachedGeometry>& GetAllVisibleNodes() const { return PhysicalNodes; }

	/** Retrieve the last reported physical geometry for the specified node, if available */
	TOptional<FCachedGeometry> GetPhysicalGeometryForNode(const FDisplayNodeRef& InNode) const;

	/** Report geometry for a child row */
	void ReportChildRowGeometry(const FDisplayNodeRef& InNode, const FGeometry& InGeometry);

	/** Called when a child row widget has been added/removed */
	void OnChildRowRemoved(const FDisplayNodeRef& InNode);

protected:
	
	/** Linear, sorted array of nodes that we currently have generated widgets for */
	TArray<FCachedGeometry> PhysicalNodes;

	/** Map of cached geometries for visible nodes */
	TMap<FDisplayNodeRef, FCachedGeometry> CachedRowGeometry;

protected:

	/** Populate the map of column definitions, and add relevant columns to the header row */
	void SetupColumns(const FArguments& InArgs);

	/** Ensure that the track area column is either show or hidden, depending on the visibility of the curve editor */
	void UpdateTrackArea();

private:

	/** The tree view's header row (hidden) */
	TSharedPtr<SHeaderRow> HeaderRow;

	/** Pointer to the node tree data that is used to populate this tree */
	TSharedPtr<FSequencerNodeTree> SequencerNodeTree;

	/** Cached copy of the root nodes from the tree data */
	TArray<FDisplayNodeRef> RootNodes;

	/** Column definitions for each of the columns in the tree view */
	TMap<FName, FSequencerTreeViewColumn> Columns;

	/** Strong pointer to the track area so we can generate track lanes as we need them */
	TSharedPtr<SSequencerTrackArea> TrackArea;
};

/** Widget that represents a row in the sequencer's tree control. */
class SSequencerTreeViewRow	: public SMultiColumnTableRow<FDisplayNodeRef>
{
public:
	DECLARE_DELEGATE_RetVal_ThreeParams(TSharedRef<SWidget>, FOnGenerateWidgetForColumn, const FDisplayNodeRef&, const FName&, const TSharedRef<SSequencerTreeViewRow>&);

	SLATE_BEGIN_ARGS(SSequencerTreeViewRow){}

		/** Delegate to invoke to create a new column for this row */
		SLATE_EVENT(FOnGenerateWidgetForColumn, OnGenerateWidgetForColumn)

	SLATE_END_ARGS()

	~SSequencerTreeViewRow();

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const FDisplayNodeRef& InNode);

	/** Get the dispolay node to which this row relates */
	TSharedPtr<FSequencerDisplayNode> GetDisplayNode() const;

	/** Add a reference to the specified track lane, keeping it alive until this row is destroyed */
	void AddTrackAreaReference(const TSharedPtr<SSequencerTrackLane>& Lane);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;

private:

	/** Cached reference to a track lane that we relate to. This keeps the track lane alive (it's a weak widget) as long as we are in view. */
	TSharedPtr<SSequencerTrackLane> TrackLaneReference;

	/** The item associated with this row of data */
	mutable TWeakPtr<FSequencerDisplayNode> Node;

	/** Delegate to call to create a new widget for a particular column. */
	FOnGenerateWidgetForColumn OnGenerateWidgetForColumn;
};