// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IKeyArea;
class ISequencerSection;

namespace ESequencerNode
{
	enum Type
	{
		/* Top level object binding node */
		Object,
		/* Area for tracks */
		Track,
		/* Area for keys inside of a section */
		KeyArea,
		/* Displays a category */
		Category,
	};
}

/**
 * Base Sequencer layout node
 */
class FSequencerDisplayNode : public TSharedFromThis<FSequencerDisplayNode>
{
public:
	virtual ~FSequencerDisplayNode(){}
	/**
	 * Constructor
	 * 
	 * @param InNodeName	The name identifier of then node
	 * @param InParentNode	The parent of this node or NULL if this is a root node
	 * @param InParentTree	The tree this node is in
	 */
	FSequencerDisplayNode( FName InNodeName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree );

	/**
	 * Adds a category to this node
	 * 
	 * @param CategoryName	Name of the category
	 * @param DisplayName	Localized label to display in the animation outliner
	 */
	TSharedRef<class FSectionCategoryNode> AddCategoryNode( FName CategoryName, const FText& DisplayLabel );

	/**
	 * Adds a new section area for this node.
	 * 
	 * @param SectionName		Name of the section area
	 * @param AssociatedType	The track associated with sections in this node
	 */
	TSharedRef<class FTrackNode> AddSectionAreaNode( FName SectionName, UMovieSceneTrack& AssociatedTrack );

	/**
	 * Adds a key area to this node
	 *
	 * @param KeyAreaName	Name of the key area
	 * @param DisplayLabel	Localized label to display in the animation outliner
	 * @param KeyArea		Key area interface for drawing and interaction with keys
	 */
	void AddKeyAreaNode( FName KeyAreaName, const FText& DisplayLabel, TSharedRef<IKeyArea> KeyArea );

	/**
	 * @return The type of node this is
	 */
	virtual ESequencerNode::Type GetType() const = 0;

	/** @return Whether or not this node can be selected */
	virtual bool IsSelectable() const { return true; }

	/**
	 * @return The desired height of the node when displayed
	 */
	virtual float GetNodeHeight() const = 0;
	
	/**
	 * @return The localized display name of this node
	 */
	virtual FText GetDisplayName() const = 0;

	/**
	 * Generates a widget for display in the animation outliner portion of the track area
	 * 
	 * @param Sequencer	Sequencer interface to pass to outliner widgets
	 * @return Generated outliner widget
	 */
	virtual TSharedRef<SWidget> GenerateWidgetForOutliner( TSharedRef<class FSequencer> Sequencer );

	/**
	 * Generates a widget for display in the section area portion of the track area
	 * 
	 * @param ViewRange	The range of time in the sequencer that we are displaying
	 * @return Generated outliner widget
	 */
	virtual TSharedRef<SWidget> GenerateWidgetForSectionArea( const TAttribute< TRange<float> >& ViewRange );

	/**
	 * @return the path to this node starting with the outermost parent
	 */
	FString GetPathName() const;

	/** What sort of context menu this node summons */
	virtual TSharedPtr<SWidget> OnSummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) {return TSharedPtr<SWidget>();}

	/**
	 * @return The name of the node (for identification purposes)
	 */
	FName GetNodeName() const { return NodeName; }

	/**
	 * @return The number of child nodes belonging to this node
	 */
	uint32 GetNumChildren() const { return ChildNodes.Num(); }

	/**
	 * @return How deep we are in the master tree
	 */
	uint32 GetTreeLevel() const { return TreeLevel; }

	/**
	 * @return A List of all Child nodes belonging to this node
	 */
	const TArray< TSharedRef<FSequencerDisplayNode> >& GetChildNodes() const { return ChildNodes; }

	/**
	 * @return The parent of this node                                                              
	 */
	TSharedPtr<FSequencerDisplayNode> GetParent() const { return ParentNode.Pin(); }
	
	/** Gets the sequencer that owns this node */
	FSequencer& GetSequencer() const { return ParentTree.GetSequencer(); }
	
	/** Gets all the key area nodes recursively, including this node if applicable */
	virtual void GetChildKeyAreaNodesRecursively(TArray< TSharedRef<class FSectionKeyAreaNode> >& OutNodes) const;

	/**
	 * Toggles the expansion state of this node
	 */
	void ToggleExpansion();

	/**
	 * @return Whether or not this node is expanded                                                              
	 */
	bool IsExpanded() const;

	/**
	 * @return Whether or not a node is visible
	 */
	bool IsVisible() const;

	/** Updates the cached shot filtered visibility flag */
	void UpdateCachedShotFilteredVisibility();

	/** Pins this node, forcing it to the top of the sequencer. Affects visibility */
	void PinNode();
	
protected:
	/**
	 * Visibility of nodes is a complex situation. There are 7 different factors to account for:
	 * - Expansion State of Parents
	 * - Search Filtering
	 * - Shot Filtering
	 * - Clean View
	 * - Pinned Nodes
	 * - Manually keyed nodes with a single key are not displayed
	 * - Sub Movie Scene Filtering (maybe, we might just ignore this completely)
	 *
	 * It is handled by first caching the shot filtering state in each node.
	 * Manually keyed nodes with a single key are cached at this point as well.
	 * This cached state is updated only when shot filtering changes.
	 * This is done for a) Simplicity, to break things apart
	 * and b) Efficiency, since that is likely the most expensive visibility flag
	 * and, most importantly, c) because it is the only pass that is dependent on
	 * the way the tree structure is laid out (it requires looking at child nodes).
	 *
	 * Then, the remaining visibility flags are AND'd together in these groups
	 * 1) CachedShotFilteringFlag
	 * 2) Search Filtering, which overrides Expansion State of Parents
	 * 3) Clean View and Pinned Nodes, which intersects with shot filtering global enabling
	 * 4) Sub Movie Scenes (not implemented yet)
	 */

	/**
	 * Returns visibility, calculating the conditions for which this
	 * node's shot filtering visibility is cached
	 */
	virtual bool GetShotFilteredVisibilityToCache() const = 0;
	
	/** Whether this node has visible children, based on cached shot filtering visibility only */
	bool HasVisibleChildren() const;
	/** Whether this node is a root node, or it's parent is expanded */
	bool IsParentExpandedOrIsARootNode() const;
	
protected:
	/** The parent of this node*/
	TWeakPtr< FSequencerDisplayNode > ParentNode;
	/** List of children belonging to this node */
	TArray< TSharedRef<FSequencerDisplayNode > > ChildNodes;
	/** Parent tree that this node is in */
	FSequencerNodeTree& ParentTree;
	/** The name identifier of this node */
	FName NodeName;
	/** How far deep in the tree the node is */
	uint32 TreeLevel;
	/** Whether or not the node is expanded */
	bool bExpanded;
	/** A cached state of what this node's visibility is based only on shot filtering */
	bool bCachedShotFilteredVisibility;
	/** Whether this node is pinned to the top of the sequencer */
	bool bNodeIsPinned;
};

/**
 * Represents an area inside a section where keys are displayed
 * There is one key area per section that defines that key area
 */
class FSectionKeyAreaNode : public FSequencerDisplayNode
{
public:
	/**
	 * Constructor
	 * 
	 * @param InNodeName	The name identifier of then node
	 * @param InDisplayName	Display name of the category
	 * @param InParentNode	The parent of this node or NULL if this is a root node
	 * @param InParentTree	The tree this node is in
	 * @param bInTopLevel	If true the node is part of the section itself instead of taking up extra height in the section
	 */
	FSectionKeyAreaNode( FName NodeName, const FText& InDisplayName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree, bool bInTopLevel = false )
		: FSequencerDisplayNode( NodeName, InParentNode, InParentTree )
		, DisplayName( InDisplayName )
		, bTopLevel( bInTopLevel )
	{}

	/** FSequencerDisplayNodeInterface */
	virtual ESequencerNode::Type GetType() const override { return ESequencerNode::KeyArea; }
	virtual float GetNodeHeight() const override;
	virtual FText GetDisplayName() const override { return DisplayName; }
	virtual bool GetShotFilteredVisibilityToCache() const override;

	/**
	 * Adds a key area to this node
	 *
	 * @param KeyArea	The key area interface to add
	 */
	void AddKeyArea( TSharedRef<IKeyArea> KeyArea );

	/**
	 * Returns a key area at the given index
	 * 
	 * @param Index	The index of the key area to get
	 * @return the key area at the index
	 */
	TSharedRef<IKeyArea> GetKeyArea( uint32 Index ) const { return KeyAreas[Index]; }

	/**
	 * Returns all key area for this node
	 * 
	 * @return All key areas
	 */
	TArray< TSharedRef<IKeyArea> > GetAllKeyAreas() const { return KeyAreas; }

	/** @return Whether the node is top level.  (I.E., is part of the section itself instead of taking up extra height in the section) */
	bool IsTopLevel() const { return bTopLevel; }
private:
	/** The display name of the key area */
	FText DisplayName;
	/** All key areas on this node (one per section) */
	TArray< TSharedRef<IKeyArea> > KeyAreas;
	/** If true the node is part of the section itself instead of taking up extra height in the section */
	bool bTopLevel;
};


/**
 * Represents an area to display Sequencer sections (possibly on multiple lines)
 */
class FTrackNode : public FSequencerDisplayNode
{
public:
	/**
	 * Constructor
	 * 
	 * @param InNodeName	The name identifier of then node
	 * @param InDisplayName	Display name of the section area
	 * @param InParentNode	The parent of this node or NULL if this is a root node
	 * @param InParentTree	The tree this node is in
	 */
	FTrackNode( FName NodeName, UMovieSceneTrack& InAssociatedType, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree );

	/** FSequencerDisplayNodeInterface */
	virtual ESequencerNode::Type GetType() const override { return ESequencerNode::Track; }
	virtual float GetNodeHeight() const override;
	virtual FText GetDisplayName() const override;
	virtual bool GetShotFilteredVisibilityToCache() const override;
	virtual void GetChildKeyAreaNodesRecursively(TArray< TSharedRef<class FSectionKeyAreaNode> >& OutNodes) const override;

	/**
	 * Adds a section to this node
	 *
	 * @param SequencerSection	The section to add
	 */
	void AddSection( TSharedRef<ISequencerSection>& SequencerSection )
	{
		Sections.Add( SequencerSection );
	}

	/**
	 * Makes the section itself a key area without taking up extra space
	 *
	 * @param KeyArea	Interface for the key area
	 */
	void SetSectionAsKeyArea( TSharedRef<IKeyArea>& KeyArea );
	
	/**
	 * @return All sections in this node
	 */
	const TArray< TSharedRef<ISequencerSection> >& GetSections() const { return Sections; }
	TArray< TSharedRef<ISequencerSection> >& GetSections() { return Sections; }

	/** @return Returns the top level key node for the section area if it exists */
	TSharedPtr< FSectionKeyAreaNode > GetTopLevelKeyNode() { return TopLevelKeyNode; }

	/** @return the track associated with this section */
	UMovieSceneTrack* GetTrack() const { return AssociatedType.Get(); }

	/** Gets the greatest row index of all the sections we have */
	int32 GetMaxRowIndex() const;

	/** Ensures all row indices which have no sections are gone */
	void FixRowIndices();

private:
	/** All of the sequencer sections in this node */
	TArray< TSharedRef<ISequencerSection> > Sections;
	/** If the section area is a key area itself, this represents the node for the keys */
	TSharedPtr< FSectionKeyAreaNode > TopLevelKeyNode;
	/** The type associated with the sections in this node */
	TWeakObjectPtr<UMovieSceneTrack> AssociatedType;
};

/**
 * A node for displaying an object binding
 */
class FObjectBindingNode : public FSequencerDisplayNode
{
public:
	/**
	 * Constructor
	 * 
	 * @param InNodeName		The name identifier of then node
	 * @param InObjectName		The name of the object we're binding to
	 * @param InObjectBinding	Object binding guid for associating with live objects
	 * @param InParentNode		The parent of this node or NULL if this is a root node
	 * @param InParentTree		The tree this node is in
	 */
	FObjectBindingNode( FName NodeName, const FString& InObjectName, const FGuid& InObjectBinding, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
		: FSequencerDisplayNode( NodeName, InParentNode, InParentTree )
		, ObjectBinding( InObjectBinding )
		, DefaultDisplayName( FText::FromString(InObjectName) )
	{}

	/** FSequencerDisplayNodeInterface */
	virtual ESequencerNode::Type GetType() const override { return ESequencerNode::Object; }
	virtual FText GetDisplayName() const override;
	virtual float GetNodeHeight() const override;
	virtual bool GetShotFilteredVisibilityToCache() const override;
	
	/** @return The object binding on this node */
	const FGuid& GetObjectBinding() const { return ObjectBinding; }
	
	/** What sort of context menu this node summons */
	virtual TSharedPtr<SWidget> OnSummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	/** The binding to live objects */
	FGuid ObjectBinding;
	/** The default display name of the object which is used if the binding manager doesn't provide one. */
	FText DefaultDisplayName;
};

/**
 * A node that displays a category for other nodes
 */
class FSectionCategoryNode : public FSequencerDisplayNode
{
public:
	/**
	 * Constructor
	 * 
	 * @param InNodeName	The name identifier of then node
	 * @param InDisplayName	Display name of the category
	 * @param InParentNode	The parent of this node or NULL if this is a root node
	 * @param InParentTree	The tree this node is in
	 */
	FSectionCategoryNode( FName NodeName, const FText& InDisplayName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree )
		: FSequencerDisplayNode( NodeName, InParentNode, InParentTree )
		, DisplayName( InDisplayName )
	{}

	/** FSequencerDisplayNodeInterface */
	virtual ESequencerNode::Type GetType() const override { return ESequencerNode::Category; }
	virtual float GetNodeHeight() const override;
	virtual FText GetDisplayName() const override { return DisplayName; }
	virtual bool GetShotFilteredVisibilityToCache() const override;
private:
	/** The display name of the category */
	FText DisplayName;
};
