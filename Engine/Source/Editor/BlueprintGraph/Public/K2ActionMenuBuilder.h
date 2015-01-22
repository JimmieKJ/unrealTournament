// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphSchema_K2.h" // for FFunctionTargetInfo

/*******************************************************************************
* FBlueprintGraphActionListBuilder
*******************************************************************************/

struct BLUEPRINTGRAPH_API FBlueprintGraphActionListBuilder : public FGraphContextMenuBuilder
{
public:
	// The current blueprint (can be NULL)
	UBlueprint const* Blueprint;

	FBlueprintGraphActionListBuilder(const UEdGraph* InGraph);
};

/*******************************************************************************
* FBlueprintPaletteListBuilder
*******************************************************************************/

//FBlueprintPaletteListBuilder
struct BLUEPRINTGRAPH_API FBlueprintPaletteListBuilder : public FCategorizedGraphActionListBuilder
{
public:
	// The current blueprint (can be NULL)
	UBlueprint const* Blueprint;

	FBlueprintPaletteListBuilder(UBlueprint const* BlueprintIn, FString const& Category = TEXT(""));
};

/*******************************************************************************
* FK2ActionMenuBuilder
*******************************************************************************/

class BLUEPRINTGRAPH_API FK2ActionMenuBuilder
{
public:
	/** Utility to create a component node add action */
	static TSharedPtr<struct FEdGraphSchemaAction_K2AddComponent> CreateAddComponentAction(UObject* TemporaryOuter, const UBlueprint* Blueprint, TSubclassOf<UActorComponent> DestinationComponentType, UObject* Asset);

	/** Utility to create a node creating action */
	static TSharedPtr<struct FEdGraphSchemaAction_K2NewNode> AddNewNodeAction(FGraphActionListBuilderBase& ContextMenuBuilder, const FString& Category, const FText& MenuDesc, const FString& Tooltip, const int32 Grouping = 0, const FString& Keywords = FString());

	/** Utility that creates a 'new event' entry based on the function supplied. Can either override that function, or simply use the function as its signature. */
	static void AddEventForFunction(FGraphActionListBuilderBase& ActionMenuBuilder, class UFunction* Function);

	/** 
	 * Adds macro action to passed array of schema actions
	 * @param [in,out]	ActionMenuBuilder		Menu builder to append macro actions to
	 * @param [in]		MacroGraph				The macro graph to attach		
	 */
	static void AttachMacroGraphAction(FGraphActionListBuilderBase& ActionMenuBuilder, UEdGraph* MacroGraph);

	/**
	 * Converts a UFunction into a single spawn info record and adds that to the specified array.
	 *
	 * @param	Function			The function to convert.
	 * @param	bIsParentContext	Whether or not this function is being called on the parent context
	 * @param	TargetInfo			Allows spawning nodes which also create a target variable as well
	 * @param	CallOnMember		Creates a 'call on member' node instead of regular 'call function' node
	 * @param	BaseCategory		Category to put spawn action into
	 * @param   ActionGrouping		This is a priority number for overriding alphabetical order in the action list (higher value  == higher in the list)
	 * @param	PlaceOnTop			Don't use category, place the node on top
	 * @param [in,out]	ListBuilder	List builder.
	 */
	static void AddSpawnInfoForFunction(const UFunction* Function, bool bIsParentContext, struct FFunctionTargetInfo const& TargetInfo, const struct FMemberReference& CallOnMember, const FString& BaseCategory, int32 const ActionGrouping, FGraphActionListBuilderBase& ListBuilder, bool bCalledForEach = false, bool bPlaceOnTop = false);

	// This variable allows potentially unsafe (given the current known context) commands to appear in the context menu
	// It defaults to off, but can be enabled via a console command when creating macros for internal use, where safety
	// is guaranteed by higher level code.
	static int32 bAllowUnsafeCommands;

public:
	FK2ActionMenuBuilder(UEdGraphSchema_K2 const* Schema);

	/**
	 * Get all actions that can be performed when right clicking on a graph or 
	 * drag-releasing on a graph from a pin
	 *
	 * @param [in,out]	ContextMenuBuilder	The context (graph, dragged pin, etc...) and output menu builder.
	 */
	void GetGraphContextActions(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/** Function that returns _all_ nodes we could place */
	void GetAllActions(FBlueprintPaletteListBuilder& PaletteBuilder) const;
	
	/** Helper method to add items valid to the palette list */
	void GetPaletteActions(FBlueprintPaletteListBuilder& ActionMenuBuilder, TWeakObjectPtr<UClass> FilterClass = NULL) const;

private:
	/** Appends the kinds of nodes that can be created given a context */
	void GetContextAllowedNodeTypes(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;
	/** */
	void GetPinAllowedNodeTypes(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/**
	 * Returns a list of spawn info structures corresponding to all function call nodes that can be created in the specified context.
	 *
	 * @param [in,out]	ListBuilder	List builder.
	 * @param	Class  				Class to get nodes from; NULL indicates to search global function libraries.
	 * @param	OwnerClass  		Class to build list for, used to identify blueprint class when browsing global function libraries.
	 * @param	DestGraph			Graph we will be adding action for (may be NULL)
	 * @param	FunctionTypes		Combination of EFunctionType to indicate types of functions required
	 * @param	BaseCategory		Category to add function nodes to
	 * @param	TargetInfo			Allows spawning nodes which also create a target variable as well
	 * @param	bShowInherited
	 * @param	bCalledForEach		Call for each element in an array (a node accepts array)
	 */
	void GetFuncNodesForClass(FGraphActionListBuilderBase& ListBuilder, const UClass* Class, const UClass* OwnerClass,
		const UEdGraph* DestGraph, const uint32 FunctionTypes, const FString& BaseCategory, 
		const FFunctionTargetInfo& TargetInfo = FFunctionTargetInfo(), bool bShowInherited = true, bool bCalledForEach = false) const;

	/** Get all nodes we can place based on the supplied class */
	void GetAllActionsForClass(FBlueprintPaletteListBuilder& PaletteBuilder, const UClass* InClass, bool bShowInherited, bool bShowProtected, bool bShowVariables, bool bShowDelegates) const;

	/** Helper method to load all available switch statements */
	void GetSwitchMenuItems(FBlueprintPaletteListBuilder& ActionMenuBuilder, FString const& RootCategory, UEdGraphPin const* PinContext = NULL) const;

	/** */
	void GetAnimNotifyMenuItems(struct FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/**
	 * Look at all member variables of a specified class and create getter/setter spawn infos for
	 * ones that pass k2/editable filters.
	 *
	 * @param	InClass				The class to search.
	 * @param	bSelfContext		true if the self pin should be the blueprint's self, false if it should reference InClass
	 * @param	bIncludeDelegates	Whether or now we want to include delegates in the list of variables
	 * @param [in,out]	OutTypes	Array to append spawn actions to for each eligible variable.
	 */
	void GetVariableGettersSettersForClass(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, bool bSelfContext, bool bIncludeDelegates) const;

	/**
	 * Look at all member variables of a specified class and create getter/setter spawn infos for
	 * ones that pass k2/editable and (optionally) pin type filters.
	 *
	 * @param	InClass				  	The class to search.
	 * @param	DesiredPinType		  	Type of the desired pin.
	 * @param	bSelfContext			true if the self pin should be the blueprint's self, false if it should reference InClass
	 * @param	bWantGetters		  	Should Getters (variable reads) be included?
	 * @param	bWantSetters		  	Should Setters (variable writers) be included?
	 * @param	bWantDelegates			Should delegate binders be included?
	 * @param	bRequireDesiredPinType	(optional) type of the require desired pin.
	 * @param [in,out]	OutTypes	  	Array to append spawn actions to for each eligible variable.
	 */
	void GetVariableGettersSettersForClass(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, const FEdGraphPinType& DesiredPinType, bool bSelfContext, bool bWantGetters, bool bWantSetters, bool bWantDelegates, bool bRequireDesiredPinType) const;

	/** Look at all member variables of a specified class and create EventDispatcher nodes spawn infos for ones that pass k2/editable filters */
	void GetEventDispatcherNodesForClass(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, bool bSelfContext) const;

	/** Get nodes for structure make/break */
	void GetStructActions(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/** Finds all valid event actions */
	void GetAllEventActions(FBlueprintPaletteListBuilder& ActionMenuBuilder) const;

	/** Get all the event nodes from function overrides on this class */
	void GetEventsForBlueprint(FBlueprintPaletteListBuilder& ActionMenuBuilder) const;

	/** Generate a list of potential literal reference nodes based on the user's selected actors */
	void GetLiteralsFromActorSelection(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/** Generate a list of potential bound events to add based on the user's selected actors */
	void GetBoundEventsFromActorSelection(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/** Add bound event actions for an actor or blueprint property */
	void AddBoundEventActionsForClass(FBlueprintGraphActionListBuilder& ContextMenuBuilder, UClass* BindClass, const FString& CurrentCategory, TArray<AActor*>& Actors, UObjectProperty* ComponentProperty) const;

	/** Get potential matinee controller nodes */
	void GetMatineeControllers(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/** Get functions you can call on the selected actor */
	void GetFunctionCallsOnSelectedActors(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/** Get events that can be bound to the component this property points to */
	void GetBoundEventsFromComponentSelection(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/** Get functions you can call on the selected actor */
	void GetFunctionCallsOnSelectedComponents(FBlueprintGraphActionListBuilder& ContextMenuBuilder) const;

	/** Gets all the macro tools action nodes */
	void GetMacroTools(FBlueprintPaletteListBuilder& ActionMenuBuilder, const bool bInAllowImpureFuncs, const UEdGraph* InCurrentGraph) const;

	/**
	 * Finds all K2 callable macros that can be associated with this pin.
	 * @param [in,out]	ContextMenuBuilder		Menu builder to append macro actions to
	 * @param	Class							The class to check if macros are compatible with 
	 * @param	DesiredPinType					The type of the pin we're matching to
	 * @param	bWantOutputs					If true, require the matching macro's pin to be an output, otherwise it must be an input.
	 * @param bAllowImpureMacros				Allow impure macros?
	 */
	void GetPinAllowedMacros(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, const FEdGraphPinType& DesiredPinType, bool bWantOutputs, bool bAllowImpureMacros) const;

	/**
	 * Finds all k2-callable functions with a pin that matches the specified type and direction and pureness
	 *
	 * @param	Class  				Class to get functions with the supplied pin type
	 * @param	OwnerClass  		Class to build list for, used to identify blueprint class when browsing global function libraries.
	 * @param	InGraph				Graph that we want to add node to
	 * @param	DesiredPinType  	Type of the desired pin.
	 * @param	bWantOutput			If true, require the pin to be an output, otherwise it must be an input.
	 * @param [in,out]	OutTypes	Array to append the spawn action struct to.
	 * @param   bPureOnly           If true, only pure func nodes will be considered
	 */
	void GetFuncNodesWithPinType(FBlueprintGraphActionListBuilder& ContextMenuBuilder, const UClass* Class, class UClass* OwnerClass, const FEdGraphPinType& DesiredPinType, bool bWantOutput, bool bPureOnly) const;

	/** Finds all valid interface message actions */
	void GetAllInterfaceMessageActions(FGraphActionListBuilderBase& ActionMenuBuilder) const;

private:
	/** */
	UEdGraphSchema_K2 const* K2Schema;
};

