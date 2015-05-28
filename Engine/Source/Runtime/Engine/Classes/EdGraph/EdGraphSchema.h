// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema.generated.h"

class FSlateRect;
class UEdGraphNode;
class UEdGraphPin;
struct FEdGraphPinType;

/** Distinguishes between different graph types. Graphs can have different properties; for example: functions have one entry point, ubergraphs can have multiples. */
UENUM()
enum EGraphType
{
	GT_Function,
	GT_Ubergraph,
	GT_Macro,
	GT_Animation,
	GT_StateMachine,
	GT_MAX,
};

/** This is the type of response the graph editor should take when making a connection */
UENUM()
enum ECanCreateConnectionResponse
{
	// Make the connection; there are no issues (message string is displayed if not empty)
	CONNECT_RESPONSE_MAKE,

	// Cannot make this connection; display the message string as an error
	CONNECT_RESPONSE_DISALLOW,

	// Break all existing connections on A and make the new connection (it's exclusive); display the message string as a warning/notice
	CONNECT_RESPONSE_BREAK_OTHERS_A,

	// Break all existing connections on B and make the new connection (it's exclusive); display the message string as a warning/notice
	CONNECT_RESPONSE_BREAK_OTHERS_B,

	// Break all existing connections on A and B, and make the new connection (it's exclusive); display the message string as a warning/notice
	CONNECT_RESPONSE_BREAK_OTHERS_AB,

	// Make the connection via an intermediate cast node, or some other conversion node
	CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE,

	CONNECT_RESPONSE_MAX,
};

/** This structure represents a context dependent action, with sufficient information for the schema to perform it */
USTRUCT()
struct ENGINE_API FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction"); return Type;}
	virtual FName GetTypeId() const { return StaticGetTypeId(); }

	/** The menu text that should be displayed for this node in the creation menu */
	UPROPERTY()
	FText MenuDescription;

	/** The tooltip text that should be displayed for this node in the creation menu */
	UPROPERTY()
	FString TooltipDescription;

	/** This is the UI centric category the action fits in (e.g., Functions, Variables).  Use this instead of the NodeType.NodeCategory because multiple NodeCategories might visually belong together. */
	UPROPERTY()
	FString Category;

	/** This is just an arbitrary dump of extra text that search will match on, in addition to the description and tooltip, e.g., Add might have the keyword Math */
	UPROPERTY()
	FText Keywords;

	/** This is a priority number for overriding alphabetical order in the action list (higher value  == higher in the list) */
	UPROPERTY()
	int32 Grouping;

	/** Section ID of the action list in which this action belongs */
	UPROPERTY()
	int32 SectionID;

	/** Search title for the action (doesn't have to be set when instantiated, will be constructed by GetSearchTitle() if left empty)*/
	UPROPERTY()
	FText CachedSearchTitle;

	/** Search keywords for the action (doesn't have to be set when instantiated, will be constructed by GetSearchTitle() if left empty)*/
	UPROPERTY()
	FText CachedSearchKeywords;

	FEdGraphSchemaAction() 
		: Grouping(0)
		, SectionID(0)
	{}
	
	virtual ~FEdGraphSchemaAction() {}

	FEdGraphSchemaAction(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: MenuDescription(InMenuDesc)
		, TooltipDescription(InToolTip)
		, Category(InNodeCategory)
		, Grouping(InGrouping)
		, SectionID(0)
	{
	}

	/** Whether or not this action can be parented to other actions of the same type */
	virtual bool IsParentable() const { return false; }

	/** Execute this action, given the graph and schema, and possibly a pin that we were dragged from. Returns a node that was created by this action (if any).  */
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) { return NULL; }

	/** Execute this action, given the graph and schema, and possibly a pin that we were dragged from. Returns a node that was created by this action (if any).  */
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true)
	{
		UEdGraphNode* NewNode = NULL;
		if (FromPins.Num() > 0)
		{
			NewNode = PerformAction(ParentGraph, FromPins[0], Location, bSelectNewNode);
		}
		else
		{
			NewNode = PerformAction(ParentGraph, NULL, Location, bSelectNewNode);
		}

		return NewNode;
	}

	/** Retrieves the full searchable title for this action */
	FText GetSearchTitle()
	{
		if(CachedSearchTitle.IsEmpty())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("LocalizedTitle"), MenuDescription);
			Args.Add(TEXT("SourceTitle"), FText::FromString(MenuDescription.BuildSourceString()));
			CachedSearchTitle = FText::Format(FText::FromString("{LocalizedTitle} {SourceTitle}"), Args);
		}
		return CachedSearchTitle;
	}

	/** Retrieves the full searchable title for this action */
	FText GetSearchKeywords()
	{
		if(CachedSearchKeywords.IsEmpty())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("LocalizedKeywords"), Keywords);
			Args.Add(TEXT("SourceKeywords"), FText::FromString(Keywords.BuildSourceString()));
			CachedSearchKeywords = FText::Format(FText::FromString("{LocalizedKeywords} {SourceKeywords}"), Args);
		}
		return CachedSearchKeywords;
	}

	// GC.
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
};

/** Action to add a node to the graph */
USTRUCT()
struct ENGINE_API FEdGraphSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_NewNode"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	/** Template of node we want to create */
	UPROPERTY()
	class UEdGraphNode* NodeTemplate;


	FEdGraphSchemaAction_NewNode() 
		: FEdGraphSchemaAction()
		, NodeTemplate(NULL)
	{}

	FEdGraphSchemaAction_NewNode(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) 
		, NodeTemplate(NULL)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FEdGraphSchemaAction interface

	template <typename NodeType>
	static NodeType* SpawnNodeFromTemplate(class UEdGraph* ParentGraph, NodeType* InTemplateNode, const FVector2D Location, bool bSelectNewNode = true)
	{
		FEdGraphSchemaAction_NewNode Action;
		Action.NodeTemplate = InTemplateNode;

		return Cast<NodeType>(Action.PerformAction(ParentGraph, NULL, Location, bSelectNewNode));
	}

	static UEdGraphNode* CreateNode(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, class UEdGraphNode* NodeTemplate);
};

/** Dummy action, useful for putting messages in the menu */
struct FEdGraphSchemaAction_Dummy : public FEdGraphSchemaAction
{
	static FName StaticGetTypeId() { static FName Type("FEdGraphSchemaAction_Dummy"); return Type; }
	virtual FName GetTypeId() const override { return StaticGetTypeId(); }

	FEdGraphSchemaAction_Dummy()
	: FEdGraphSchemaAction()
	{}

	FEdGraphSchemaAction_Dummy(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping)		
	{}
};

/** This is a response from CanCreateConnection, indicating if the connecting action is legal and what the result will be */
struct FPinConnectionResponse
{
public:
	FText Message;

	TEnumAsByte<enum ECanCreateConnectionResponse> Response;

public:
	FPinConnectionResponse()
	: Message(FText::GetEmpty())
	, Response(CONNECT_RESPONSE_MAKE)
	{
	}

	FPinConnectionResponse(const ECanCreateConnectionResponse InResponse, const FString& InMessage)
		: Message(FText::FromString(InMessage))
		, Response(InResponse)
	{
	}

	FPinConnectionResponse(const ECanCreateConnectionResponse InResponse, const ANSICHAR* InMessage)
		: Message(FText::FromString(InMessage))
		, Response(InResponse)
	{
	}

	FPinConnectionResponse(const ECanCreateConnectionResponse InResponse, const WIDECHAR* InMessage)
		: Message(FText::FromString(InMessage))
		, Response(InResponse)
	{
	}

	FPinConnectionResponse(const ECanCreateConnectionResponse InResponse, const FText& InMessage)
		: Message(InMessage)
		, Response(InResponse)
	{
	}

	friend bool operator==(const FPinConnectionResponse& A, const FPinConnectionResponse& B)
	{
		return (A.Message.ToString() == B.Message.ToString()) && (A.Response == B.Response);
	}	

	/** If a connection can be made without breaking existing connections */
	bool CanSafeConnect() const
	{
		return (Response == CONNECT_RESPONSE_MAKE);
	}
};


// This object is a base class helper used when building a list of actions for some menu or palette
struct FGraphActionListBuilderBase
{
public:
	/** A single entry in the list - can contain multiple actions */
	class ActionGroup
	{
	public:
		/** Constructor accepting a single action */
		ENGINE_API ActionGroup( TSharedPtr<FEdGraphSchemaAction> InAction, FString const& RootCategory = TEXT("") );

		/** Constructor accepting multiple actions */
		ActionGroup( const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions, FString const& RootCategory = TEXT("") );

		/**
		 * Concatenates RootCategory with the first action's category (RootCategory
		 * coming first, as a prefix, and the splits the category hierarchy apart 
		 * into separate entries.
		 * 
		 * @param  HierarchyOut	A list of the category tiers that this action should be listed under.
		 */
		ENGINE_API void GetCategoryChain(TArray<FString>& HierarchyOut) const;

		/**
		 * Goes through all actions and calls PerformAction on them individually
		 * @param ParentGraph The graph we are operating in the context of.
		 * @param FromPins Optional pins that the action was dragged from.
		 * @param Location The position on the graph to place new nodes.
		 */
		ENGINE_API void PerformAction( class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location );
		
		/** All of the actions this entry contains */
		TArray< TSharedPtr<FEdGraphSchemaAction> > Actions;

	private:
		/** The category to list this entry under (could be left empty, as it gets concatenated with the first sub-action's category) */
		FString RootCategory;
	};
private:

	/** All of the action entries */
	TArray< ActionGroup > Entries;

public:
	/** Adds an action entry containing a single action */
	ENGINE_API virtual void AddAction( const TSharedPtr<FEdGraphSchemaAction>& NewAction, FString const& Category = TEXT("") );

	/** Adds an action entry containing multiple actions */
	ENGINE_API virtual void AddActionList( const TArray<TSharedPtr<FEdGraphSchemaAction> >& NewActions, FString const& Category = TEXT("") );

	/** Appends all the action entries from a different graph action builder */
	ENGINE_API void Append( FGraphActionListBuilderBase& Other );

	/** Returns the current number of entries */
	ENGINE_API int32 GetNumActions() const;

	/** Returns the specified entry */
	ENGINE_API ActionGroup& GetAction( int32 Index );

	/** Clears the action entries */
	ENGINE_API virtual void Empty();

	// The temporary graph outer to store any template nodes created
	UEdGraph* OwnerOfTemporaries;

public:
	template<typename NodeType>
	NodeType* CreateTemplateNode(UClass* Class=NodeType::StaticClass())
	{
		return NewObject<NodeType>(OwnerOfTemporaries, Class);
	}

	FGraphActionListBuilderBase()
		: OwnerOfTemporaries(NULL)
	{
	}
};

/** Used to nest all added action under one root category */
struct FCategorizedGraphActionListBuilder : public FGraphActionListBuilderBase
{
public:
	ENGINE_API FCategorizedGraphActionListBuilder(FString const& Category = TEXT(""));

	// FGraphActionListBuilderBase Interface
	ENGINE_API virtual void AddAction(const TSharedPtr<FEdGraphSchemaAction>& NewAction, FString const& Category = TEXT("") ) override;
	ENGINE_API virtual void AddActionList(const TArray<TSharedPtr<FEdGraphSchemaAction> >& NewActions, FString const& Category = TEXT("")) override;
	// End of FGraphActionListBuilderBase Interface

private:
	/** An additional category that we want all actions listed under (ok if left empty) */
	FString Category;
};

// This context is used when building a list of actions that can be done in the current blueprint
struct FGraphActionMenuBuilder : public FGraphActionListBuilderBase
{
public:
	const UEdGraphPin* FromPin;
public:
	ENGINE_API FGraphActionMenuBuilder() : FromPin(NULL) {}
};

// This context is used when building a list of actions that can be done in the current context
struct FGraphContextMenuBuilder : public FGraphActionMenuBuilder
{
public:
	// The current graph (will never be NULL)
	const UEdGraph* CurrentGraph;

	// The selected objects
	TArray<UObject*> SelectedObjects;
public:
	ENGINE_API FGraphContextMenuBuilder(const UEdGraph* InGraph);
};

/** This is a response from GetGraphDisplayInformation */
struct FGraphDisplayInfo
{
public:
	/** Plain name for this graph */
	FText PlainName;
	/** Friendly name to display for this graph */
	FText DisplayName;
	/** Text to show as tooltip for this graph */
	FString Tooltip;
	/** Optional link to big tooltip documentation for this graph */
	FString DocLink;
	/** Excerpt within doc for big tooltip */
	FString DocExcerptName;

	TArray<FString> Notes;
public:
	FGraphDisplayInfo() 
	{}

	ENGINE_API FString GetNotesAsString() const;
};


UCLASS(abstract)
class ENGINE_API UEdGraphSchema : public UObject
{
	GENERATED_UCLASS_BODY()


	/**
	 * Get all actions that can be performed when right clicking on a graph or drag-releasing on a graph from a pin
	 *
	 * @param [in,out]	ContextMenuBuilder	The context (graph, dragged pin, etc...) and output menu builder.
	 */
	 virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const;

	/**
	 * Gets actions that should be added to the right-click context menu for a node or pin
	 * 
	 * @param	CurrentGraph		The current graph.
	 * @param	InGraphNode			The node to get the context menu for, if any.
	 * @param	InGraphPin			The pin clicked on, if any, to provide additional context
	 * @param	MenuBuilder			The menu builder to append actions to.
	 * @param	bIsDebugging		Is the graph editor currently part of a debugging session (any non-debugging commands should be disabled)
	 */
	 virtual void GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const;

	/**
	 * Determine if a connection can be created between two pins.
	 *
	 * @param	A	The first pin.
	 * @param	B	The second pin.
	 *
	 * @return	An empty string if the connection is legal, otherwise a message describing why the connection would fail.
	 */
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Not implemented by this schema"));
	}

	/**
	 * Determine if two nodes can be merged
	 *
	 * @param	A	The first node.
	 * @param	B	The second node.
	 *
	 * @return	An empty string if the merge is legal, otherwise a message describing why the merge would fail.
	 */
	virtual const FPinConnectionResponse CanMergeNodes(const UEdGraphNode* A, const UEdGraphNode* B) const 
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Not implemented by this schema"));
	}


	// Categorizes two pins into an input pin and an output pin.  Returns true if successful or false if they don't make sense as such (two inputs or two outputs)
	template<typename PinType>
	static bool CategorizePinsByDirection(PinType* PinA, PinType* PinB, /*out*/ PinType*& InputPin, /*out*/ PinType*& OutputPin)
	{
		InputPin = NULL;
		OutputPin = NULL;

		if ((PinA->Direction == EGPD_Input) && (PinB->Direction == EGPD_Output))
		{
			InputPin = PinA;
			OutputPin = PinB;
			return true;
		}
		else if ((PinB->Direction == EGPD_Input) && (PinA->Direction == EGPD_Output))
		{
			InputPin = PinB;
			OutputPin = PinA;
			return true;
		}
		else
		{
			return false;
		}
	}

	/**
	 * Try to make a connection between two pins.
	 *
	 * @param	A	The first pin.
	 * @param	B	The second pin.
	 *
	 * @return	True if a connection was made/broken (graph was modified); false if the connection failed and had no side effects.
	 */
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const;

	/**
	 * Try to create an automatic cast or other conversion node node to facilitate a connection between two pins.
	 * It makes the cast node, a connection between A and the cast node, and a connection from the cast node to B.two 
	 * This method is called when a connection is made where CanCreateConnection returned bCanAutoConvert.
	 *
	 *
	 * @param	A	The first pin.
	 * @param	B	The second pin.
	 *
	 * @return	True if a cast node and connection were made; false if the connection failed and had no side effects.
	 */
	virtual bool CreateAutomaticConversionNodeAndConnections(UEdGraphPin* A, UEdGraphPin* B) const;

	/**
	 * Determine if the supplied pin default values would be valid.
	 *
	 * @param	Pin			   	The pin to check the default value on.
	 *
	 * @return	An empty string if the new value is legal, otherwise a message describing why it is invalid.
	 */
	virtual FString IsPinDefaultValid(const UEdGraphPin* Pin, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InNewDefaultText) const  { return TEXT("Not implemented by this schema"); }

	/** 
	 *	Determine whether the current pin default values are valid
	 *	@see IsPinDefaultValid
	 */
	FString IsCurrentPinDefaultValid(const UEdGraphPin* Pin) const;

	/**
	 * An easy way to check to see if the current graph system supports pin watching.
	 * 
	 * @return True if the schema supports pin watching, false if not.
	 */
	virtual bool DoesSupportPinWatching() const	{ return false; }

	/**
	 * Checks to see if the specified pin is being watched by the graph's debug system.
	 * 
	 * @param  Pin	The pin you want to check for.
	 * @return True if the supplied pin is currently being watched, false if not.
	 */
	virtual bool IsPinBeingWatched(UEdGraphPin const* Pin) const { return false; }

	/**
	 * If the specified pin is currently being watched, then this will clear the 
	 * watch from the graph's debug system.
	 * 
	 * @param  Pin	The pin you wish to no longer watch.
	 */
	virtual void ClearPinWatch(UEdGraphPin const* Pin) const {}

	/**
	 * Sets the string to the specified pin; even if it is invalid it is still set.
	 *
	 * @param	Pin			   	The pin on which to set the default value.
	 * @param	NewDefaultValue	The new default value.
	 */
	virtual void TrySetDefaultValue(UEdGraphPin& Pin, const FString& NewDefaultValue) const;

	/** Sets the object to the specified pin */
	virtual void TrySetDefaultObject(UEdGraphPin& Pin, UObject* NewDefaultObject) const;

	/** Sets the text to the specified pin */
	virtual void TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText) const;

	/** If we should disallow viewing and editing of the supplied pin */
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const { return false; }

	/** Should the Pin in question display an asset picker */
	virtual bool ShouldShowAssetPickerForPin(UEdGraphPin* Pin) const { return true; }

	/**
	 * Gets the draw color of a pin based on it's type.
	 *
	 * @param	PinType	The type to convert into a color.
	 *
	 * @return	The color representing the passed in type.
	 */
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const { return FLinearColor::Black; }

	/** Get the name to show in the editor */
	virtual FText GetPinDisplayName(const UEdGraphPin* Pin) const;

	/**
	 * Takes the PinDescription and tacks on any other data important to the 
	 * schema (things like the pin's type, etc.). The base one here just spits 
	 * back the PinDescription.
	 * 
	 * @param   Pin				The pin you want a tool-tip generated for
	 * @param   PinDescription	A detailed description, describing the pin's purpose
	 * @param   TooltipOut		The constructed tool-tip (out)
	 */
	virtual void ConstructBasicPinTooltip(UEdGraphPin const& Pin, FText const& PinDescription, FString& TooltipOut) const;

	/** @return     The type of graph (function vs. ubergraph) that this that TestEdGraph is. */
	//@TODO: This is too K2-specific to be included in EdGraphSchema and should be refactored
	virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const { return GT_Function; }

	/**
	 * Query if the passed in pin is a title bar pin.
	 *
	 * @param	Pin	The pin to check.
	 *
	 * @return	true if the pin should appear in the title bar.
	 */
	virtual bool IsTitleBarPin(const UEdGraphPin& Pin) const { return false; }

	/**
	 * Breaks all links from/to a single node
	 *
	 * @param	TargetNode	The node to break links on
	 */
	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const;

	/** */
	static bool SetNodeMetaData(UEdGraphNode* Node, FName const& KeyValue);

	/**
	 * Breaks all links from/to a single pin
	 *
	 * @param	TargetPin	The pin to break links on
	 * @param	bSendsNodeNotifcation	whether to send a notification to the node post pin connection change
	 */
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const;

	/**
	 * Breaks the link between two nodes.
	 *
	 * @param	SourcePin	The pin where the link begins.
	 * @param	TargetLink	The pin where the link ends.
	 */
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin);

	/** Split a pin in to subelements */
	virtual void SplitPin(UEdGraphPin* Pin) const { };

	/** Collapses a pin and its siblings back in to the original pin */
	virtual void RecombinePin(UEdGraphPin* Pin) const { };

	/** Handles double-clicking on a pin<->pin connection */
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const { }

	/** Break links on this pin and create links instead on MoveToPin */
	virtual FPinConnectionResponse MovePinLinks(UEdGraphPin& MoveFromPin, UEdGraphPin& MoveToPin, bool bIsIntermediateMove = false) const;

	/** Copies pin links from one pin to another without breaking the original links */
	virtual FPinConnectionResponse CopyPinLinks(UEdGraphPin& CopyFromPin, UEdGraphPin& CopyToPin, bool bIsIntermediateCopy = false) const;

	/** Is self pin type? */
	virtual bool IsSelfPin(const UEdGraphPin& Pin) const   {return false;}

	/** Is given string a delegate category name ? */
	virtual bool IsDelegateCategory(const FString& Category) const   {return false;}

	/** 
	 * Populate new graph with any default nodes
	 * 
	 * @param	Graph			Graph to add the default nodes to
	 * @param	ContextClass	If specified, the graph terminators will use this class to search for context for signatures (i.e. interface functions)
	 */
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const {}

	/**
	 * Reconstructs a node
	 *
	 * @param	TargetNode	The node to reconstruct
	 * @param	bIsBatchRequest	If true, this reconstruct node is part of a batch.  Allows subclasses to defer marking classes as dirty until they are all done
	 */
	virtual void ReconstructNode(UEdGraphNode& TargetNode, bool bIsBatchRequest=false) const;

	/**
	 * Attempts to construct a substitute node that is unique within its graph
	 *
	 * @param	Node			The node to replace
	 * @param	Graph			The destination graph
	 * @param	InstanceGraph	Object instancing graph
	 * @return					NULL if a substitute node cannot be created; otherwise, the substitute node instance
	 */
	virtual UEdGraphNode* CreateSubstituteNode(UEdGraphNode* Node, const UEdGraph* Graph, FObjectInstancingGraph* InstanceGraph) const { return NULL; }


	/**
	 * Returns the currently selected graph node count
	 *
	 * @param	Graph			The active graph to find the selection count for
	 */
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const { return 0; }

	/** Returns schema action to create comment from implemention */
	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const { return NULL; }

	/** Returns schema action to create documention node from implemention */
	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateDocumentNodeAction() const { return NULL; }

	/**
	 * Handle a graph being removed by the user (potentially removing associated bound nodes, etc...)
	 */
	virtual void HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const {}

	/**
	 * Can TestNode be encapsulated into a child graph?
	 */
	virtual bool CanEncapuslateNode(UEdGraphNode const& TestNode) const { return true; }

	/**
	 * Gets display information for a graph
	 *
	 * @param	Graph				Graph to get information on
	 * @param	[out] DisplayInfo	Appropriate display info for Graph
	 */
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const;

	/** Called when asset(s) are dropped onto a graph background. */
	virtual void DroppedAssetsOnGraph(const TArray<class FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const {}

	/** Called when asset(s) are dropped onto the specified node */
	virtual void DroppedAssetsOnNode(const TArray<class FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const {}

	/** Called when asset(s) are dropped onto the specified pin */
	virtual void DroppedAssetsOnPin(const TArray<class FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const {}

	/** Allows schema to generate a tooltip (icon & message) when the specified asset(s) are dragged over the specified node */
	virtual void GetAssetsNodeHoverMessage(const TArray<class FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const 
	{ 
		OutTooltipText = TEXT("");
		OutOkIcon = false;
	}

	/** Allows schema to generate a tooltip (icon & message) when the specified asset(s) are dragged over the specified pin */
	virtual void GetAssetsPinHoverMessage(const TArray<class FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const 
	{ 
		OutTooltipText = TEXT("");
		OutOkIcon = false;
	}

	/** Allows schema to generate a tooltip (icon & message) when the specified asset(s) are dragged over the specified graph */
	virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const
	{
		OutTooltipText = TEXT("");
		OutOkIcon = false;
	}

	/* Can this graph type be duplicated? */
	virtual bool CanDuplicateGraph(UEdGraph* InSourceGraph) const { return true; }

	/* Duplicate a given graph return the duplicate graph */
	virtual UEdGraph* DuplicateGraph(UEdGraph* GraphToDuplicate) const { return NULL;}

	/* returns new FConnectionDrawingPolicy from this schema */
	virtual class FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const { return NULL; }

	/* When dragging off a pin, we want to duck the alpha of some nodes */
	virtual bool FadeNodeWhenDraggingOffPin(const UEdGraphNode* Node, const UEdGraphPin* Pin) const { return false; }

	virtual void BackwardCompatibilityNodeConversion(UEdGraph* Graph, bool bOnlySafeChanges) const { }

	/* When a node is removed, this method determines whether we should remove it immediately or use the old (slower) code path that results in all node being recreated: */
	virtual bool ShouldAlwaysPurgeOnModification() const { return true; }

	/*
	 * Some schemas have nodes that support the user dynamically adding pins when dropping a connection on the node
	 *
	 * @param InTargetNode					Node to check for pin adding support
	 * @param InSourcePinName				Name of the pin being dropped, a new pin of similar name will be constructed
	 * @param InSourcePinType				Type of pin to drop onto the node
	 * @param InSourcePinDirection			Direction of the source pin
	 * @return								Returns the new pin if created
	 */
	virtual UEdGraphPin* DropPinOnNode(UEdGraphNode* InTargetNode, const FString& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const { return nullptr; }

	/**
	 * Checks if the node supports dropping a pin on it
	 *
	 * @param InTargetNode					Node to check for pin adding support
	 * @param InSourcePinType				Type of pin to drop onto the node
	 * @param InSourcePinDirection			Direction of the source pin
	 * @param OutErrorMessage				Only filled with an error if there is pin add support but there is an error with the pin type
	 * @return								Returns TRUE if there is support for dropping the pin on the node
	 */
	virtual bool SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const { return false; }

	/**
	 * Checks if a CacheRefreshID is out of date
	 *
	 * @param InVisualizationCacheID	The current refresh ID to check if out of date
	 * @return							TRUE if dirty
	 */
	virtual bool IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const { return false; }

	/** Returns the current cache title refresh ID that is appropriate for the passed node */
	virtual int32 GetCurrentVisualizationCacheID() const { return 0; }

	/** Forces cached visualization data to refresh */
	virtual void ForceVisualizationCacheClear() const {};
};
