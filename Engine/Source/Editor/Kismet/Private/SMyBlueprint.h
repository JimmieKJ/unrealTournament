// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DiffUtils.h"

/* Enums to use when grouping the blueprint members in the list panel. The order here will determine the order in the list */
namespace NodeSectionID
{
	enum Type
	{
		NONE = 0,
		GRAPH,					// Graph
		FUNCTION,				// Functions
		FUNCTION_OVERRIDABLE,	// Overridable functions
		INTERFACE,				// Interface
		MACRO,					// Macros
		VARIABLE,				// Variables
		COMPONENT,				// Components
		DELEGATE,				// Delegate/Event
		USER_ENUM,				// User defined enums
		LOCAL_VARIABLE,			// Local variables
		USER_STRUCT,			// User defined structs
		USER_SORTED				// User sorted categories
	};
};

class FMyBlueprintCommands : public TCommands<FMyBlueprintCommands>
{
public:
	/** Constructor */
	FMyBlueprintCommands() 
		: TCommands<FMyBlueprintCommands>("MyBlueprint", NSLOCTEXT("Contexts", "My Blueprint", "My Blueprint"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	// Basic operations
	TSharedPtr<FUICommandInfo> OpenGraph;
	TSharedPtr<FUICommandInfo> OpenGraphInNewTab;
	TSharedPtr<FUICommandInfo> FocusNode;
	TSharedPtr<FUICommandInfo> FocusNodeInNewTab;
	TSharedPtr<FUICommandInfo> ImplementFunction;
	TSharedPtr<FUICommandInfo> FindEntry;
	TSharedPtr<FUICommandInfo> DeleteEntry;
	TSharedPtr<FUICommandInfo> GotoNativeVarDefinition;
	// Add New Item
	/** Initialize commands */
	virtual void RegisterCommands() override;
};

class SMyBlueprint : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMyBlueprint ) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FBlueprintEditor> InBlueprintEditor, const UBlueprint* InBlueprint = nullptr);
	void SetInspector( TSharedPtr<SKismetInspector> InInspector ) { Inspector = InInspector ; }

	/* Reset the last pin type settings to default. */
	void ResetLastPinType();

	/** Refreshes the graph action menu */
	void Refresh();
	void SetFocusedGraph(UEdGraph* InEdGraph) { EdGraph = InEdGraph; }
	
	/** Accessor for getting the current selection as a K2 graph */
	FEdGraphSchemaAction_K2Graph* SelectionAsGraph() const;

	/** Accessor for getting the current selection as a K2 enum */
	FEdGraphSchemaAction_K2Enum* SelectionAsEnum() const;

	/** Accessor for getting the current selection as a K2 enum */
	FEdGraphSchemaAction_K2Struct* SelectionAsStruct() const;

	/** Accessor for getting the current selection as a K2 var */
	FEdGraphSchemaAction_K2Var* SelectionAsVar() const;
	
	/** Accessor for getting the current selection as a K2 delegate */
	FEdGraphSchemaAction_K2Delegate* SelectionAsDelegate() const;

	/** Accessor for getting the current selection as a K2 event */
	FEdGraphSchemaAction_K2Event* SelectionAsEvent() const;

	/** Accessor for getting the current selection as a K2 local var */
	FEdGraphSchemaAction_K2LocalVar* SelectionAsLocalVar() const;

	/** Accessor for determining if the current selection is a category*/
	bool SelectionIsCategory() const;
	
	void EnsureLastPinTypeValid();

	/** Gets the last pin type selected by this widget, or by the function editor */
	FEdGraphPinType& GetLastPinTypeUsed() {EnsureLastPinTypeValid(); return LastPinType;}
	FEdGraphPinType& GetLastFunctionPinTypeUsed() {EnsureLastPinTypeValid(); return LastFunctionPinType;}

	/** Accessor the blueprint object from the main editor */
	UBlueprint* GetBlueprintObj() const {return Blueprint;}

	/** Gets whether we are showing user variables only or not */
	bool ShowUserVarsOnly() const {return bShowUserVarsOnly;}

	/** Gets our parent blueprint editor */
	TWeakPtr<FBlueprintEditor> GetBlueprintEditor() {return BlueprintEditorPtr;}

	/**
	 * Fills the supplied array with the currently selected objects
	 * @param OutSelectedItems The array to fill.
	 */
	void GetSelectedItemsForContextMenu(TArray<FComponentEventConstructionData>& OutSelectedItems) const;

	/** Called to reset the search filter */
	void OnResetItemFilter();

	/** Selects an item by name in either the main graph action menu or the local one */
	void SelectItemByName(const FName& ItemName, ESelectInfo::Type SelectInfo = ESelectInfo::Direct, int32 SectionId = INDEX_NONE, bool bIsCategory = false);

	/** Clears the selection in the graph action menus */
	void ClearGraphActionMenuSelection();

	/** Initiates a rename on the selected action node, if possible */
	void OnRequestRenameOnActionNode();

	/** Expands any category with the associated name */
	void ExpandCategory(const FString& CategoryName);

	/** Move the category before the target category */
	bool MoveCategoryBeforeCategory( const FString& CategoryToMove, const FString& TargetCategory );
private:
	/** Creates widgets for the graph schema actions */
	TSharedRef<SWidget> OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData);

	/** Creates the local variable action list sub-widget */
	TSharedRef<SWidget> ConstructLocalActionPanel();

	/** Callback used to populate all actions list in SGraphActionMenu */
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);
	void GetChildGraphs(UEdGraph* EdGraph, FGraphActionListBuilderBase& OutAllActions, FString ParentCategory = FString());
	void GetChildEvents(UEdGraph const* EdGraph, int32 const SectionId, FGraphActionListBuilderBase& OutAllActions) const;
	void GetLocalVariables(FGraphActionListBuilderBase& OutAllActions) const;
	
	/** Handles the visibility of the local action list */
	EVisibility GetLocalActionsListVisibility() const;

	/** Callbacks for the graph action menu */
	FReply OnActionDragged(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions, const FPointerEvent& MouseEvent);
	FReply OnCategoryDragged(const FString& InCategory, const FPointerEvent& MouseEvent);
	void OnActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions);
	static void OnActionSelectedHelper(TSharedPtr<FEdGraphSchemaAction> InAction, UBlueprint* Blueprint, TSharedRef<SKismetInspector> Inspector);
	void OnGlobalActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions);
	void OnLocalActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions);
	void OnActionDoubleClicked(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions);
	TSharedPtr<SWidget> OnContextMenuOpening();

	void OnCategoryNameCommitted(const FText& InNewText, ETextCommit::Type InTextCommit, TWeakPtr< struct FGraphActionNode > InAction );
	bool CanRequestRenameOnActionNode(TWeakPtr<struct FGraphActionNode> InSelectedNode) const;
	FText OnGetSectionTitle( int32 InSectionID );

	/** Support functions for checkbox to manage displaying user variables only */
	ECheckBoxState OnUserVarsCheckState() const;
	void OnUserVarsCheckStateChanged(ECheckBoxState InNewState);
	
	/** Helper function to open the selected graph */
	void OpenGraph(FDocumentTracker::EOpenDocumentCause InCause);

	/** Callbacks for commands */
	void OnOpenGraph();
	void OnOpenGraphInNewTab();
	bool CanOpenGraph() const;
	bool CanFocusOnNode() const;
	void OnFocusNode();
	void OnFocusNodeInNewTab();
	void OnImplementFunction();
	bool CanImplementFunction() const;
	void OnFindEntry();
	bool CanFindEntry() const;
	void OnDeleteEntry();
	bool CanDeleteEntry() const;
	FReply OnAddNewLocalVariable();
	bool CanRequestRenameOnActionNode() const;
	bool IsDuplicateActionVisible() const;
	bool CanDuplicateAction() const;
	void OnDuplicateAction();
	void GotoNativeCodeVarDefinition();
	bool IsNativeVariable() const;

	/** Callback when the filter is changed, forces the action tree(s) to filter */
	void OnFilterTextChanged( const FText& InFilterText );

	/** Callback for the action trees to get the filter text */
	FText GetFilterText() const;

	/** Checks if the selected action has context menu */
	bool SelectionHasContextMenu() const;

	/** Update Node Create Analytic */
	void UpdateNodeCreation();

	/** Returns the displayed category, if any, of a graph */
	FString GetGraphCategory(UEdGraph* InGraph) const;

	/** Helper function to delete a graph in the MyBlueprint window */
	void OnDeleteGraph(UEdGraph* InGraph, EEdGraphSchemaAction_K2Graph::Type);

	/** Helper function to delete a delegate in the MyBlueprint window */
	void OnDeleteDelegate(FEdGraphSchemaAction_K2Delegate* InDelegateAction);

	UEdGraph* GetFocusedGraph() const;
private:
	/** Pointer back to the blueprint editor that owns us */
	TWeakPtr<FBlueprintEditor> BlueprintEditorPtr;
	
	/** Graph Action Menu for displaying all our variables and functions */
	TSharedPtr<class SGraphActionMenu> GraphActionMenu;

	/** Graph Action Menu for displaying all local variables */
	TSharedPtr<class SGraphActionMenu> LocalGraphActionMenu;

	/** Whether or not we're only showing user-created variables */
	bool bShowUserVarsOnly;

	/** The last pin type used (including the function editor last pin type) */
	FEdGraphPinType LastPinType;
	FEdGraphPinType LastFunctionPinType;

	/** Enums created from 'blueprint' level */
	TArray<TWeakObjectPtr<UUserDefinedEnum>> EnumsAddedToBlueprint;

	/** The filter box that handles filtering for both graph action menus. */
	TSharedPtr< SSearchBox > FilterBox;

	/** Contains both the GraphActionMenu and LocalGraphActionMenu */
	TSharedPtr< SSplitter > ActionMenuContainer;

	/** Enums created from 'blueprint' level */
	TArray<TWeakObjectPtr<UUserDefinedStruct>> StructsAddedToBlueprint;

	/** The blueprint being displayed: */
	UBlueprint* Blueprint;

	/** The Ed Graph being displayed: */
	UEdGraph* EdGraph;

	/** The Kismet Inspector used to display properties: */
	TWeakPtr<SKismetInspector> Inspector;
};
