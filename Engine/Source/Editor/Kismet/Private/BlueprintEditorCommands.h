// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FBlueprintEditorCommands

/** Set of kismet 2 wide commands */
class FBlueprintEditorCommands : public TCommands<FBlueprintEditorCommands>
{
public:

	FBlueprintEditorCommands()
		: TCommands<FBlueprintEditorCommands>( TEXT("BlueprintEditor"), NSLOCTEXT("Contexts", "BlueprintEditor", "Blueprint Editor"), NAME_None, FEditorStyle::GetStyleSetName() )
	{
	}	

	virtual void RegisterCommands() override;


	// File-ish commands
	TSharedPtr< FUICommandInfo > CompileBlueprint;
	TSharedPtr< FUICommandInfo > RefreshAllNodes;
	TSharedPtr< FUICommandInfo > DeleteUnusedVariables;
	TSharedPtr< FUICommandInfo > FindInBlueprints;

	TSharedPtr< FUICommandInfo > FindReferencesFromClass;
	TSharedPtr< FUICommandInfo > FindReferencesFromBlueprint;
	TSharedPtr< FUICommandInfo > RepairCorruptedBlueprint;

	// Edit commands
	TSharedPtr< FUICommandInfo > FindInBlueprint;
	TSharedPtr< FUICommandInfo > ReparentBlueprint;

	// View commands
	TSharedPtr< FUICommandInfo > ZoomToWindow;
	TSharedPtr< FUICommandInfo > ZoomToSelection;
	TSharedPtr< FUICommandInfo > NavigateToParent;
	TSharedPtr< FUICommandInfo > NavigateToParentBackspace;
	TSharedPtr< FUICommandInfo > NavigateToChild;

	// Preview commands
	TSharedPtr< FUICommandInfo > ResetCamera;
	TSharedPtr< FUICommandInfo > EnableSimulation;
	TSharedPtr< FUICommandInfo > ShowFloor;
	TSharedPtr< FUICommandInfo > ShowGrid;

	// Debugging commands
	TSharedPtr< FUICommandInfo > EnableAllBreakpoints;
	TSharedPtr< FUICommandInfo > DisableAllBreakpoints;
	TSharedPtr< FUICommandInfo > ClearAllBreakpoints;
	TSharedPtr< FUICommandInfo > ClearAllWatches;

	// New documents
	TSharedPtr< FUICommandInfo > AddNewVariable;
	TSharedPtr< FUICommandInfo > AddNewLocalVariable;
	TSharedPtr< FUICommandInfo > AddNewFunction;
	TSharedPtr< FUICommandInfo > AddNewMacroDeclaration;
	TSharedPtr< FUICommandInfo > AddNewAnimationGraph;
	TSharedPtr< FUICommandInfo > AddNewEventGraph;
	TSharedPtr< FUICommandInfo > AddNewDelegate;

	// Development commands
	TSharedPtr< FUICommandInfo > SaveIntermediateBuildProducts;
	TSharedPtr< FUICommandInfo > RecompileGraphEditor;
	TSharedPtr< FUICommandInfo > RecompileKismetCompiler;
	TSharedPtr< FUICommandInfo > RecompileBlueprintEditor;
	TSharedPtr< FUICommandInfo > RecompilePersona;
	TSharedPtr< FUICommandInfo > GenerateNativeCode;

	// SSC commands
	TSharedPtr< FUICommandInfo > BeginBlueprintMerge;
};

//////////////////////////////////////////////////////////////////////////
// FBlueprintSpawnNodeCommands

/** Handles spawn node commands for the Blueprint Editor */
class FBlueprintSpawnNodeCommands : public TCommands<FBlueprintSpawnNodeCommands>
{
public:

	FBlueprintSpawnNodeCommands()
		: TCommands<FBlueprintSpawnNodeCommands>(TEXT("BlueprintEditorSpawnNodes"), NSLOCTEXT("Contexts", "BlueprintEditor_SpawnNodes", "Blueprint Editor - Spawn Nodes by gesture"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}	

	virtual void RegisterCommands() override;

	/**
	 * Returns a graph action assigned to the passed in gesture
	 *
	 * @param InGesture			The gesture to use for lookup
	 * @param InPaletteBuilder	The Blueprint palette to create the graph action for, used for validation purposes and to link any important node data to the blueprint
	 * @param InDestGraph		The graph to create the graph action for, used for validation purposes and to link any important node data to the graph
	 */
	void GetGraphActionByGesture(FInputGesture& InGesture, struct FBlueprintPaletteListBuilder& InPaletteBuilder, UEdGraph* InDestGraph) const;

private:
	/** An array of all the possible commands for spawning nodes */
	TArray< TSharedPtr< class FNodeSpawnInfo > > NodeCommands;
};
