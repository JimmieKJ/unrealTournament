// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "EditorStyleSet.h"

class FGraphEditorCommandsImpl : public TCommands<FGraphEditorCommandsImpl>
{
public:

	FGraphEditorCommandsImpl()
		: TCommands<FGraphEditorCommandsImpl>( TEXT("GraphEditor"), NSLOCTEXT("Contexts", "GraphEditor", "Graph Editor"), NAME_None, FEditorStyle::GetStyleSetName() )
	{
	}	

	virtual ~FGraphEditorCommandsImpl()
	{
	}

	GRAPHEDITOR_API virtual void RegisterCommands() override;

	TSharedPtr< FUICommandInfo > ReconstructNodes;
	TSharedPtr< FUICommandInfo > BreakNodeLinks;

	// Execution sequence specific commands
	TSharedPtr< FUICommandInfo > AddExecutionPin;
	TSharedPtr< FUICommandInfo > RemoveExecutionPin;

	// SetFieldsInStruct specific commands
	TSharedPtr< FUICommandInfo > RemoveThisStructVarPin;
	TSharedPtr< FUICommandInfo > RemoveOtherStructVarPins;
	TSharedPtr< FUICommandInfo > RestoreAllStructVarPins;

	// Select node specific commands
	TSharedPtr< FUICommandInfo > AddOptionPin;
	TSharedPtr< FUICommandInfo > RemoveOptionPin;
	TSharedPtr< FUICommandInfo > ChangePinType;

	// Pin visibility modes
	TSharedPtr< FUICommandInfo > ShowAllPins;
	TSharedPtr< FUICommandInfo > HideNoConnectionPins;
	TSharedPtr< FUICommandInfo > HideNoConnectionNoDefaultPins;

	// Event / Function Entry commands
	TSharedPtr< FUICommandInfo > AddParentNode;

	// Debugging commands
	TSharedPtr< FUICommandInfo > RemoveBreakpoint;
	TSharedPtr< FUICommandInfo > AddBreakpoint;
	TSharedPtr< FUICommandInfo > EnableBreakpoint;
	TSharedPtr< FUICommandInfo > DisableBreakpoint;
	TSharedPtr< FUICommandInfo > ToggleBreakpoint;

	// Encapsulation commands
	TSharedPtr< FUICommandInfo > CollapseNodes;
	TSharedPtr< FUICommandInfo > PromoteSelectionToFunction;
	TSharedPtr< FUICommandInfo > PromoteSelectionToMacro;
	TSharedPtr< FUICommandInfo > ExpandNodes;
	TSharedPtr< FUICommandInfo > CollapseSelectionToFunction;
	TSharedPtr< FUICommandInfo > CollapseSelectionToMacro;

	//
	TSharedPtr< FUICommandInfo > SelectReferenceInLevel;
	TSharedPtr< FUICommandInfo > AssignReferencedActor;

	// Find variable references
	TSharedPtr< FUICommandInfo > FindVariableReferences;

	// Goto native code actions
	TSharedPtr< FUICommandInfo > GotoNativeFunctionDefinition;
	TSharedPtr< FUICommandInfo > GotoNativeVariableDefinition;

	// Jumps to a call function node's graph
	TSharedPtr< FUICommandInfo > GoToDefinition;

	// Pin-specific actions
	TSharedPtr< FUICommandInfo > BreakPinLinks;
	TSharedPtr< FUICommandInfo > PromoteToVariable;
	TSharedPtr< FUICommandInfo > SplitStructPin;
	TSharedPtr< FUICommandInfo > RecombineStructPin;
	TSharedPtr< FUICommandInfo > StartWatchingPin;
	TSharedPtr< FUICommandInfo > StopWatchingPin;

	// SkeletalControl specific commands
	TSharedPtr< FUICommandInfo > SelectBone;
	// Blend list options
	TSharedPtr< FUICommandInfo > AddBlendListPin;
	TSharedPtr< FUICommandInfo > RemoveBlendListPin;

	// options for sequence/evaluator converter
	TSharedPtr< FUICommandInfo > ConvertToSeqEvaluator;
	TSharedPtr< FUICommandInfo > ConvertToSeqPlayer;

	// options for blendspace sequence/evaluator converter
	TSharedPtr< FUICommandInfo > ConvertToBSEvaluator;
	TSharedPtr< FUICommandInfo > ConvertToBSPlayer;

	// option for opening the asset related to the graph node
	TSharedPtr< FUICommandInfo > OpenRelatedAsset;

	//create a comment node
	TSharedPtr< FUICommandInfo > CreateComment;

	// Find instances of a Custom event node
	TSharedPtr< FUICommandInfo > FindInstancesOfCustomEvent;

	// Zoom in and out on the graph editor
	TSharedPtr< FUICommandInfo > ZoomIn;
	TSharedPtr< FUICommandInfo > ZoomOut;

	// Go to node documentation
	TSharedPtr< FUICommandInfo > GoToDocumentation;
};

class GRAPHEDITOR_API FGraphEditorCommands
{
public:
	static void Register();

	static const FGraphEditorCommandsImpl& Get();

	static void Unregister();
};


