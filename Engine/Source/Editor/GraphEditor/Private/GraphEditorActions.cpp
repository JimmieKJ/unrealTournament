// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "GraphEditorActions.h"


void FGraphEditorCommandsImpl::RegisterCommands()
{
	UI_COMMAND( ReconstructNodes, "Refresh Nodes", "Refreshes nodes", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( BreakNodeLinks, "Break Link(s)", "Breaks links", EUserInterfaceActionType::Button, FInputGesture() )
	
	UI_COMMAND( AddExecutionPin, "Add execution pin", "Adds another execution output pin to an execution sequence or switch node", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( RemoveExecutionPin, "Remove execution pin", "Removes an execution output pin from an execution sequence or switch node", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( RemoveThisStructVarPin, "Remove this struct variable pin", "Removes the selected input pin", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( RemoveOtherStructVarPins, "Remove all other pins", "Removes all variable input pins, except for the selected one", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( RestoreAllStructVarPins, "Restore all structure pins", "Restore all structure pins", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( AddOptionPin, "Add Option Pin", "Adds another option input pin to the node", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( RemoveOptionPin, "Remove Option Pin", "Removes the last option input pin from the node", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( ChangePinType, "Change Pin Type", "Changes the type of this pin (boolean, int, etc.)", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( ShowAllPins, "Show All Pins", "Shows all pins", EUserInterfaceActionType::RadioButton, FInputGesture() )
	UI_COMMAND( HideNoConnectionPins, "Hide Unconnected Pins", "Hides all pins with no connections", EUserInterfaceActionType::RadioButton, FInputGesture() )
	UI_COMMAND( HideNoConnectionNoDefaultPins, "Hide Unused Pins", "Hides all pins with no connections and no default value", EUserInterfaceActionType::RadioButton, FInputGesture() )

	UI_COMMAND( AddParentNode, "Add call to parent function", "Adds a node that calls this function's parent", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( ToggleBreakpoint, "Toggle breakpoint", "Adds or removes a breakpoint on each selected node", EUserInterfaceActionType::Button, FInputGesture(EKeys::F9) )
	UI_COMMAND( AddBreakpoint, "Add breakpoint", "Adds a breakpoint to each selected node", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( RemoveBreakpoint, "Remove breakpoint", "Removes any breakpoints on each selected node", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( EnableBreakpoint, "Enable breakpoint", "Enables any breakpoints on each selected node", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( DisableBreakpoint, "Disable breakpoint", "Disables any breakpoints on each selected node", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( CollapseNodes, "Collapse Nodes", "Collapses selected nodes into a single node", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( PromoteSelectionToFunction, "Promote to Function", "Promotes selected collapsed graphs to functions.", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( PromoteSelectionToMacro, "Promote to Macro", "Promotes selected collapsed graphs to macros.", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( ExpandNodes, "Expand Node", "Expands composite nodes", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( CollapseSelectionToFunction, "Collapse to Function", "Collapses selected nodes into a single function node.", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( CollapseSelectionToMacro, "Collapse to Macro", "Collapses selected nodes into a single macro node.", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( SelectReferenceInLevel, "Find Actor in Level", "Select the actor referenced by this node in the level", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( AssignReferencedActor, "Assign selected Actor", "Assign the selected actor to be this node's referenced object", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( FindVariableReferences, "Find Variable References", "Find references of this variable", EUserInterfaceActionType::Button, FInputGesture() )
	
	UI_COMMAND( GotoNativeFunctionDefinition, "Goto Code Definition", "Goto the native code definition of this function", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( GotoNativeVariableDefinition, "Goto Code Definition", "Goto the native code definition of this variable", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( GoToDefinition, "Goto Definition", "Jumps to the graph this node is defined in if available.", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( BreakPinLinks, "Break Link(s)", "Breaks pin links", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( PromoteToVariable, "Promote to Variable", "Promotes something to a variable", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( SplitStructPin, "Split Struct Pin", "Breaks a struct pin in to a separate pin per element", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( RecombineStructPin, "Recombine Struct Pin", "Takes struct pins that have been broken in to composite elements and combines them back to a single struct pin", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( StartWatchingPin, "Watch this value", "Adds this pin or variable to the watch list", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( StopWatchingPin, "Stop watching this value", "Removes this pin or variable from the watch list ", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( SelectBone, "Select Bone", "Assign or change the bone for SkeletalControls", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( AddBlendListPin, "Add Blend Pin", "Add Blend Pin to BlendList", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( RemoveBlendListPin, "Remove Blend Pin", "Remove Blend Pin", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( ConvertToSeqEvaluator, "Convert To Single Frame Animation", "Convert to one frame animation that requires position", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( ConvertToSeqPlayer, "Convert to Sequence Player", "Convert back to sequence player without manual position set up", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( ConvertToBSEvaluator, "Convert To Single Frame BlendSpace", "Convert to one frame BlendSpace that requires position", EUserInterfaceActionType::Button, FInputGesture() )
	UI_COMMAND( ConvertToBSPlayer, "Convert to BlendSpace Player", "Convert back to BlendSpace player without manual position set up", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( OpenRelatedAsset, "Open Asset", "Opens the asset related to this node", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( EditTunnel, "Edit Tunnel", "Edit input and output pins for tunnel", EUserInterfaceActionType::Button, FInputGesture() )

	UI_COMMAND( CreateComment, "Create Comment", "Create a comment box", EUserInterfaceActionType::Button, FInputGesture(EKeys::C))

	UI_COMMAND( FindInstancesOfCustomEvent, "Find Instances of Event", "Find the instances of this custom event", EUserInterfaceActionType::Button, FInputGesture() )
}



void FGraphEditorCommands::Register()
{
	return FGraphEditorCommandsImpl::Register();
}

const FGraphEditorCommandsImpl& FGraphEditorCommands::Get()
{
	return FGraphEditorCommandsImpl::Get();
}

void FGraphEditorCommands::Unregister()
{
	return FGraphEditorCommandsImpl::Unregister();
}
