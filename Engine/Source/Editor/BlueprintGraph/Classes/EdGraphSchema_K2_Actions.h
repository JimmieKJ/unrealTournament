// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node.h"
#include "EdGraphSchema_K2_Actions.generated.h"

/*******************************************************************************
* FEdGraphSchemaAction_K2NewNode
*******************************************************************************/

/** Action to add a node to the graph */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2NewNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2NewNode"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	/** Template of node we want to create */
	UPROPERTY()
	class UK2Node* NodeTemplate;

	UPROPERTY()
	bool bGotoNode;

	FEdGraphSchemaAction_K2NewNode() 
		: FEdGraphSchemaAction()
		, NodeTemplate(NULL)
		, bGotoNode(false)
	{}

	FEdGraphSchemaAction_K2NewNode(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) 
		, NodeTemplate(NULL)
		, bGotoNode(false)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FEdGraphSchemaAction interface

	template <typename NodeType>
	static NodeType* SpawnNodeFromTemplate(class UEdGraph* ParentGraph, NodeType* InTemplateNode, const FVector2D Location, bool bSelectNewNode = true)
	{
		FEdGraphSchemaAction_K2NewNode Action;
		Action.NodeTemplate = InTemplateNode;

		return Cast<NodeType>(Action.PerformAction(ParentGraph, NULL, Location, bSelectNewNode));
	}

	static UEdGraphNode* CreateNode(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, class UK2Node* NodeTemplate, bool bSelectNewNode);
};

/*******************************************************************************
* FEdGraphSchemaAction_K2ViewNode
*******************************************************************************/

/** Action to view a node to the graph */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2ViewNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2ViewNode"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	/** node we want to view */
	UPROPERTY()
	const UK2Node* NodePtr;

	FEdGraphSchemaAction_K2ViewNode() 
		: FEdGraphSchemaAction()
		, NodePtr(NULL)
	{}

	FEdGraphSchemaAction_K2ViewNode(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) 
		, NodePtr(NULL)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface

};

/*******************************************************************************
* FEdGraphSchemaAction_K2AssignDelegate
*******************************************************************************/

/** Action to add a node to the graph */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2AssignDelegate : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2AssignDelegate"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2AssignDelegate() 
		:FEdGraphSchemaAction_K2NewNode()
	{}

	FEdGraphSchemaAction_K2AssignDelegate(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping) 
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface

	static UEdGraphNode* AssignDelegate(class UK2Node* NodeTemplate, class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode);
};

/*******************************************************************************
* FEdGraphSchemaAction_EventFromFunction
*******************************************************************************/

/** Action to add a node to the graph */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_EventFromFunction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_EventFromFunction"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	UPROPERTY()
	class UFunction* SignatureFunction;

	FEdGraphSchemaAction_EventFromFunction() 
		:FEdGraphSchemaAction()
		, SignatureFunction(NULL)
	{}

	FEdGraphSchemaAction_EventFromFunction(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping) 
		, SignatureFunction(NULL)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2AddComponent
*******************************************************************************/

/** Action to add an 'add component' node to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddComponent : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2AddComponent"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	/** Class of component we want to add */
	UPROPERTY()
		TSubclassOf<class UActorComponent> ComponentClass;

	/** Option asset to assign to newly created component */
	UPROPERTY()
	class UObject* ComponentAsset;

	FEdGraphSchemaAction_K2AddComponent()
		: FEdGraphSchemaAction_K2NewNode()
		, ComponentClass(NULL)
		, ComponentAsset(NULL)
	{}

	FEdGraphSchemaAction_K2AddComponent(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
		, ComponentClass(NULL)
		, ComponentAsset(NULL)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2AddTimeline
*******************************************************************************/

/** Action to add a 'timeline' node to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddTimeline : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2AddTimeline"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2AddTimeline()
		: FEdGraphSchemaAction_K2NewNode()
	{}

	FEdGraphSchemaAction_K2AddTimeline(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2AddEvent
*******************************************************************************/

/** Action to add a 'event' node to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddEvent : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2AddEvent"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2AddEvent()
		: FEdGraphSchemaAction_K2NewNode()
	{}

	FEdGraphSchemaAction_K2AddEvent(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface

	/** */
	BLUEPRINTGRAPH_API bool EventHasAlreadyBeenPlaced(UBlueprint const* Blueprint, class UK2Node_Event const** FoundEventOut = NULL) const;
};

/*******************************************************************************
* FEdGraphSchemaAction_K2AddCustomEvent
*******************************************************************************/

/** Action to add a 'custom event' node to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddCustomEvent : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2AddCustomEvent"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2AddCustomEvent()
		: FEdGraphSchemaAction_K2NewNode()
	{}

	FEdGraphSchemaAction_K2AddCustomEvent(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2AddCallOnActor
*******************************************************************************/

/** Action to add a 'call function on actor(s)' set of nodes to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddCallOnActor : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2AddCallOnActor"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	/** Pointer to actors in level we want to call function on */
	UPROPERTY()
		TArray<class AActor*> LevelActors;

	FEdGraphSchemaAction_K2AddCallOnActor()
		: FEdGraphSchemaAction_K2NewNode()
	{}

	FEdGraphSchemaAction_K2AddCallOnActor(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FEdGraphSchemaAction interface	
};

/*******************************************************************************
* FEdGraphSchemaAction_K2AddCallOnVariable
*******************************************************************************/

/** Action to add a 'call function on variable' pair of nodes to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddCallOnVariable : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	/** Property name we want to call the function on */
	UPROPERTY()
	FName VariableName;

	FEdGraphSchemaAction_K2AddCallOnVariable()
		: FEdGraphSchemaAction_K2NewNode()
	{}

	FEdGraphSchemaAction_K2AddCallOnVariable(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface	
};

/*******************************************************************************
* FEdGraphSchemaAction_K2AddComment
*******************************************************************************/

/** Action to add a 'comment' node to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddComment : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2AddComment"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2AddComment()
		: FEdGraphSchemaAction()
	{
		Category = TEXT("");
		MenuDescription = NSLOCTEXT("K2AddComment", "AddComment", "Add Comment...");
		TooltipDescription = NSLOCTEXT("K2AddComment", "AddComment_Tooltip", "Create a resizable comment box.").ToString();
	}

	FEdGraphSchemaAction_K2AddComment(const FText& InDescription, const FString& InToolTip)
		: FEdGraphSchemaAction()
	{
		Category = TEXT("");
		MenuDescription = InDescription;
		TooltipDescription = InToolTip;
	}

	// FEdGraphSchemaAction interface
	BLUEPRINTGRAPH_API virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2AddDocumentation
*******************************************************************************/

/** Action to add a 'documentation' node to the graph */
USTRUCT()
struct FEdGraphSchemaAction_K2AddDocumentation : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2AddDocumentation"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2AddDocumentation()
		: FEdGraphSchemaAction()
	{
		Category = TEXT("");
		MenuDescription = NSLOCTEXT("K2AddDocumentation", "AddDocumentation", "Add Documentation ...");
		TooltipDescription = NSLOCTEXT("K2AddDocumentation", "AddDocumentation_Tooltip", "Creates a Documentation Node.").ToString();
	}

	FEdGraphSchemaAction_K2AddDocumentation(const FString& InDescription, const FString& InToolTip)
		: FEdGraphSchemaAction()
	{
		Category = TEXT("");
		MenuDescription = FText::FromString( InDescription );
		TooltipDescription = InToolTip;
	}
	
	// FEdGraphSchemaAction interface
	BLUEPRINTGRAPH_API virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2TargetNode
*******************************************************************************/

/** Action to target a specific node on graph */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2TargetNode : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()


	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2TargetNode"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2TargetNode()
		: FEdGraphSchemaAction_K2NewNode()
	{}

	FEdGraphSchemaAction_K2TargetNode(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2PasteHere
*******************************************************************************/

/** Action to paste at this location on graph*/
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2PasteHere : public FEdGraphSchemaAction_K2NewNode
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2PasteHere"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2PasteHere ()
		: FEdGraphSchemaAction_K2NewNode()
	{}

	FEdGraphSchemaAction_K2PasteHere (const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction_K2NewNode(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	// FEdGraphSchemaAction interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2Enum
*******************************************************************************/

/** Reference to an enumeration (only used in 'docked' palette) */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2Enum : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2Enum"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	UEnum* Enum;

	void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		if( Enum )
		{
			Collector.AddReferencedObject(Enum);
		}
	}

	FName GetPathName() const
	{
		return Enum ? FName(*Enum->GetPathName()) : NAME_None;
	}

	FEdGraphSchemaAction_K2Enum() 
		: FEdGraphSchemaAction()
	{}

	FEdGraphSchemaAction_K2Enum (const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}
};

/*******************************************************************************
* FEdGraphSchemaAction_K2Var
*******************************************************************************/

/** Reference to a variable (only used in 'docked' palette) */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2Var : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

private:
	/** Name of function or class */
	FName		VarName;

	/** The class that owns this item */
	TWeakObjectPtr<UClass>		OwningClass;

	/** TRUE if the variable's type is boolean */
	bool bIsVarBool;

public:
	void SetVariableInfo(const FName& InVarName, const UClass* InOwningClass, bool bInIsVarBool)
	{
		VarName = InVarName;
		bIsVarBool = bInIsVarBool;

		check(InOwningClass);
		OwningClass = InOwningClass;
	}

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2Var"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2Var() 
		: FEdGraphSchemaAction()
	{}

	FEdGraphSchemaAction_K2Var (const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	FName GetVariableName() const
	{
		return VarName;
	}

	FString GetFriendlyVariableName() const
	{
		return FName::NameToDisplayString( VarName.ToString(), bIsVarBool );
	}

	UClass* GetVariableClass() const
	{
		return OwningClass.Get();
	}

	UProperty* GetProperty() const
	{
		UClass* SearchScope = GetVariableClass();
		return FindField<UProperty>(SearchScope, VarName);
	}

	// FEdGraphSchemaAction interface
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2LocalVar
*******************************************************************************/

/** Reference to a local variable (only used in 'docked' palette) */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2LocalVar : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

private:
	/** Name of function or class */
	FName		VarName;

	/** The struct that owns this item */
	TWeakObjectPtr<UStruct>		VariableScope;

	/** TRUE if the variable's type is boolean */
	bool bIsVarBool;

public:
	void SetVariableInfo(const FName& InVarName, const UStruct* InVariableScope, bool bInIsVarBool)
	{
		VarName = InVarName;
		bIsVarBool = bInIsVarBool;

		check(InVariableScope);
		VariableScope = InVariableScope;
	}

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2LocalVar"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FEdGraphSchemaAction_K2LocalVar() 
		: FEdGraphSchemaAction()
	{}

	FEdGraphSchemaAction_K2LocalVar (const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping)
	{}

	FName GetVariableName() const
	{
		return VarName;
	}

	FString GetFriendlyVariableName() const
	{
		return FName::NameToDisplayString( VarName.ToString(), bIsVarBool );
	}

	UStruct* GetVariableScope() const
	{
		return VariableScope.Get();
	}

	UProperty* GetProperty() const
	{
		UStruct* SearchScope = GetVariableScope();
		return FindField<UProperty>(SearchScope, VarName);
	}

	// FEdGraphSchemaAction interface
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2Graph
*******************************************************************************/

/** The graph type that the schema is */
UENUM()
namespace EEdGraphSchemaAction_K2Graph
{
	enum Type
	{
		Graph,
		Subgraph,
		Function,
		Interface,
		Macro,
		MAX
	};
}

/** Reference to a function, macro, event graph, or timeline (only used in 'docked' palette) */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2Graph : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2Graph"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	/** Name of function or class */
	FName FuncName;

	/** The type of graph that action is */
	EEdGraphSchemaAction_K2Graph::Type GraphType;

	/** The associated editor graph for this schema */
	UEdGraph* EdGraph;

	FEdGraphSchemaAction_K2Graph() 
		: FEdGraphSchemaAction()
	{}

	FEdGraphSchemaAction_K2Graph (EEdGraphSchemaAction_K2Graph::Type InType, const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping), GraphType(InType), EdGraph(NULL)
	{}

	// FEdGraphSchemaAction interface
	virtual bool IsParentable() const override { return true; }
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2Event
*******************************************************************************/

/**
* A reference to a specific event (living inside a Blueprint graph)... intended
* to be used the 'docked' palette only.
*/
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2Event : public FEdGraphSchemaAction_K2TargetNode
{
	GENERATED_USTRUCT_BODY()

	/**
	 * An empty default constructor (stubbed out for arrays and other containers 
	 * that need to allocate default instances of this struct)
	 */
	FEdGraphSchemaAction_K2Event() 
		: FEdGraphSchemaAction_K2TargetNode()
	{}

	/**
	 * The primary constructor, used to customize the Super FEdGraphSchemaAction.
	 * 
	 * @param   Category		The tree parent header (or path, delimited by '|') you want to sort this action under.
	 * @param   MenuDescription	The string you want displayed in the tree, corresponding to this action.
	 * @param   Tooltip			A string to display when hovering over this action entry.
	 * @param   Grouping		Used to override list ordering (actions with the same number get grouped together, higher numbers get sorted first).
	 */
	FEdGraphSchemaAction_K2Event(FString const& Category, FText const& MenuDescription, FString const& Tooltip, int32 const Grouping)
		: FEdGraphSchemaAction_K2TargetNode(Category, MenuDescription, Tooltip, Grouping)
	{}

	/**
	 * Provides a set identifier for all FEdGraphSchemaAction_K2Event actions.
	 * 
	 * @return A static identifier for actions of this type.
	 */
	static FName StaticGetTypeId() 
	{
		static FName Type("FEdGraphSchemaAction_K2Event"); 
		return Type;
	}

	// FEdGraphSchemaAction interface
	virtual FName GetTypeId() const override { return StaticGetTypeId(); }
	virtual bool IsParentable() const override { return true; }
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2InputAction
*******************************************************************************/

/**
* A reference to a specific event (living inside a Blueprint graph)... intended
* to be used the 'docked' palette only.
*/
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2InputAction : public FEdGraphSchemaAction_K2TargetNode
{
	GENERATED_USTRUCT_BODY()

	/**
	 * An empty default constructor (stubbed out for arrays and other containers
	 * that need to allocate default instances of this struct)
	 */
	FEdGraphSchemaAction_K2InputAction()
		: FEdGraphSchemaAction_K2TargetNode()
	{}

	/**
	* The primary constructor, used to customize the Super FEdGraphSchemaAction.
	*
	* @param   Category		The tree parent header (or path, delimited by '|') you want to sort this action under.
	* @param   MenuDescription	The string you want displayed in the tree, corresponding to this action.
	* @param   Tooltip			A string to display when hovering over this action entry.
	* @param   Grouping		Used to override list ordering (actions with the same number get grouped together, higher numbers get sorted first).
	*/
	FEdGraphSchemaAction_K2InputAction(FString const& Category, FText const& MenuDescription, FString const& Tooltip, int32 const Grouping)
		: FEdGraphSchemaAction_K2TargetNode(Category, MenuDescription, Tooltip, Grouping)
	{}

	/**
	* Provides a set identifier for all FEdGraphSchemaAction_K2InputAction actions.
	*
	* @return A static identifier for actions of this type.
	*/
	static FName StaticGetTypeId()
	{
		static FName Type("FEdGraphSchemaAction_K2InputAction");
		return Type;
	}

	// FEdGraphSchemaAction interface
	virtual FName GetTypeId() const override { return StaticGetTypeId(); }
	virtual bool IsParentable() const override { return true; }
	// End of FEdGraphSchemaAction interface
};

/*******************************************************************************
* FEdGraphSchemaAction_K2Delegate
*******************************************************************************/

/** Reference to a function, macro, event graph, or timeline (only used in 'docked' palette) */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2Delegate : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

private:
	/** Name of the property */
	FName DelegateName;

	/** Class that owns the delegates */
	TWeakObjectPtr<UClass> OwningClass;

public:		
	void SetDelegateInfo(const FName& InDelegateName, const UClass* InDelegateClass)
	{
		DelegateName = InDelegateName;

		check(InDelegateClass);
		OwningClass = const_cast<UClass*>(InDelegateClass);
	}

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2Delegate"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	/** The associated editor graph for this schema */
	UEdGraph* EdGraph;

	FEdGraphSchemaAction_K2Delegate() 
		: FEdGraphSchemaAction(), EdGraph(NULL)
	{}

	FEdGraphSchemaAction_K2Delegate(const FString& InNodeCategory, const FText& InMenuDesc, const FString& InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGrouping), EdGraph(NULL)
	{}

	FName GetDelegateName() const
	{
		return DelegateName;
	}

	UClass* GetDelegateClass() const
	{
		return OwningClass.Get();
	}

	UMulticastDelegateProperty* GetDelegatePoperty() const
	{
		UClass* DelegateClass = GetDelegateClass();
		return FindField<UMulticastDelegateProperty>(DelegateClass, DelegateName);
	}

	// FEdGraphSchemaAction interface
	// End of FEdGraphSchemaAction interface
};


