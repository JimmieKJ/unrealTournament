// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditor.h"
#include "Editor/GraphEditor/Public/GraphEditorDragDropAction.h"

/** DragDropAction class for dropping a Variable onto a graph */
class KISMET_API FKismetVariableDragDropAction : public FGraphEditorDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FKismetVariableDragDropAction, FGraphEditorDragDropAction)

	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnPanel(const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	virtual FReply DroppedOnAction(TSharedRef<struct FEdGraphSchemaAction> Action) override;
	virtual FReply DroppedOnCategory(FText Category) override;
	// End of FGraphEditorDragDropAction

	static TSharedRef<FKismetVariableDragDropAction> New(FName InVariableName, UStruct* InVariableSource, FNodeCreationAnalytic AnalyticCallback)
	{
		TSharedRef<FKismetVariableDragDropAction> Operation = MakeShareable(new FKismetVariableDragDropAction);
		Operation->VariableName = InVariableName;
		Operation->VariableSource = InVariableSource;
		Operation->AnalyticCallback = AnalyticCallback;
		Operation->Construct();
		return Operation;
	}

	/** Helper method to see if we're dragging in the same blueprint */
	bool IsFromBlueprint(class UBlueprint* InBlueprint) const
	{ 
		check(VariableSource.IsValid());

		UClass* VariableSourceClass = NULL;
		if(VariableSource.Get()->IsA(UClass::StaticClass()))
		{
			VariableSourceClass = CastChecked<UClass>(VariableSource.Get());
		}
		else
		{
			check(VariableSource.Get()->GetOuter());
			VariableSourceClass = CastChecked<UClass>(VariableSource.Get()->GetOuter());
		}
		UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(VariableSourceClass);
		return (InBlueprint == Blueprint);
	}

	UProperty* GetVariableProperty()
	{
		if (VariableSource.IsValid() && VariableName != NAME_None)
		{
			UProperty* VariableProperty = FindField<UProperty>(VariableSource.Get(), VariableName);
			check(VariableProperty != NULL);
			return VariableProperty;
		}
		return nullptr;
	}

	/** Set if operation is modified by alt */
	void SetAltDrag(bool InIsAltDrag) {	bAltDrag = InIsAltDrag;}

	/** Set if operation is modified by the ctrl key */
	void SetCtrlDrag(bool InIsCtrlDrag) {bControlDrag = InIsCtrlDrag;}

protected:
	 /** Construct a FKismetVariableDragDropAction */
	FKismetVariableDragDropAction();

	/** Structure for required node construction parameters */
	struct FNodeConstructionParams
	{
		FVector2D GraphPosition;
		UEdGraph* Graph;
		FName VariableName;
		TWeakObjectPtr<UStruct> VariableSource;
	};

	/** Called when user selects to create a Getter for the variable */
	static void MakeGetter(FNodeConstructionParams InParams);
	/** Called when user selects to create a Setter for the variable */
	static void MakeSetter(FNodeConstructionParams InParams);
	/** Called too check if we can execute a setter on a given property */
	static bool CanExecuteMakeSetter(FNodeConstructionParams InParams, UProperty* InVariableProperty);

	/**
	 * Test new variable type against existing links for node and get any links that will break
	 *
	 * @param	Node						The node with existing links
	 * @param	NewVariableProperty			the property for the new variable type 
	 * @param	OutBroken						All of the links which are NOT compatible with the new type
	 */
	void GetLinksThatWillBreak(UEdGraphNode* Node, UProperty* NewVariableProperty, TArray<class UEdGraphPin*>& OutBroken);

	/** Indicates if replacing the variable node, with the new property will require any links to be broken*/
	bool WillBreakLinks( UEdGraphNode* Node, UProperty* NewVariableProperty ) 
	{
		TArray<class UEdGraphPin*> BadLinks;
		GetLinksThatWillBreak(Node,NewVariableProperty,BadLinks);
		return BadLinks.Num() > 0;
	}

	/**
	 * Checks if the property can be dropped in a graph
	 *
	 * @param InVariableProperty		The variable property to check with
	 * @param InGraph					The graph to check against placing the variable
	 */
	bool CanVariableBeDropped(const UProperty* InVariableProperty, const UEdGraph& InGraph) const;


	/** Returns the local variable's scope, if any */
	UStruct* GetLocalVariableScope() const;

protected:
	/** Name of variable being dragged */
	FName VariableName;
	/** Scope this variable belongs to */
	TWeakObjectPtr<UStruct> VariableSource;
	/** Was ctrl held down at start of drag */
	bool bControlDrag;
	/** Was alt held down at the start of drag */
	bool bAltDrag;
	/** Analytic delegate to track node creation */
	FNodeCreationAnalytic AnalyticCallback;
};
