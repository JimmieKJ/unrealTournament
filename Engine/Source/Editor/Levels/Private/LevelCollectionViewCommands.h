// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** The set of commands supported by the LevelsView */
class FLevelsViewCommands : public TCommands<FLevelsViewCommands>
{

public:

	/** FLevelsViewCommands Constructor */
	FLevelsViewCommands() : TCommands<FLevelsViewCommands>
	(
		"LevelsView", // Context name for fast lookup
		NSLOCTEXT("Contexts", "LevelsView", "Levels"), // Localized context name for displaying
		"LevelEditor", // Parent
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{
	}
	
PRAGMA_DISABLE_OPTIMIZATION
	/** Initialize the commands */
	virtual void RegisterCommands() override
	{
		//selected level
		UI_COMMAND( MakeLevelCurrent, "Make Current", "Make this Level the Current Level", EUserInterfaceActionType::Button, FInputGesture( EKeys::Enter ) );
		UI_COMMAND( MoveActorsToSelected, "Move Selected Actors to Level", "Moves the selected actors to this level", EUserInterfaceActionType::Button, FInputGesture() );

		//invalid selected level
		UI_COMMAND( FixUpInvalidReference, "Replace Missing Level","Removes the broken level and replaces it with the level browsed to", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( RemoveInvalidReference, "Remove Missing Level", "Removes the reference to the missing level from the map", EUserInterfaceActionType::Button, FInputGesture() );

		//levels
		UI_COMMAND( EditProperties, "Edit Properties", "Opens the edit properties window for the selected levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SaveSelectedLevels, "Save Selected Levels", "Saves selected levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( MigrateSelectedLevels, "Migrate Selected Levels...", "Copies the selected levels and all their dependencies to a different game", EUserInterfaceActionType::Button, FInputGesture() );
		
		UI_COMMAND( DisplayActorCount, "Show Actor Count", "If enabled, displays the number of actors in each level", EUserInterfaceActionType::ToggleButton, FInputGesture() );
		UI_COMMAND( DisplayLightmassSize, "Show Lightmass Size", "If enabled, displays the Lightmass size for each level", EUserInterfaceActionType::ToggleButton, FInputGesture() );
		UI_COMMAND( DisplayFileSize, "Show File Size", "If enabled, displays the file size for each level", EUserInterfaceActionType::ToggleButton, FInputGesture() );
		UI_COMMAND( DisplayPaths, "Show Paths", "If enabled, displays the path for each level", EUserInterfaceActionType::ToggleButton, FInputGesture() );
		UI_COMMAND( DisplayEditorOffset, "Show Editor Offset", "If enabled, displays the editor offset for each level", EUserInterfaceActionType::ToggleButton, FInputGesture() );
		
		UI_COMMAND( CreateEmptyLevel, "Create Empty Level", "Creates a new empty Level", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( AddExistingLevel, "Add Existing Level", "Adds an existing level", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( AddSelectedActorsToNewLevel, "Add Selected Actors to New Level", "Adds the actors currently selected in the active viewport to a new Level", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( MergeSelectedLevels, "Merge Levels into New Level", "Merges the selected levels into a new level, removing the original levels from the persistent", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( RemoveSelectedLevels, "Remove Selected Levels", "Removes selected levels from world", EUserInterfaceActionType::Button, FInputGesture() );

		// Source Control
		UI_COMMAND( SCCCheckOut, "Check Out", "Checks out the selected asset from source control.", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SCCCheckIn, "Check In", "Checks in the selected asset to source control.", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SCCOpenForAdd, "Mark For Add", "Adds the selected asset to source control.", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SCCHistory, "History", "Displays the source control revision history of the selected asset.", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SCCRefresh, "Refresh", "Updates the source control status of the asset.", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SCCDiffAgainstDepot, "Diff Against Depot", "Look at differences between your version of the asset and that in source control.", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SCCConnect, "Connect To Source Control", "Connect to source control to allow source control operations to be performed on content and levels.", EUserInterfaceActionType::Button, FInputGesture() );

		// set new streaming method
		UI_COMMAND( SetAddStreamingMethod_Blueprint, "Set Blueprint Streaming Method", "Sets the streaming method for new or added levels to Blueprint streaming", EUserInterfaceActionType::RadioButton, FInputGesture() );
		UI_COMMAND( SetAddStreamingMethod_AlwaysLoaded, "Set Streaming to Always Loaded", "Sets the streaming method new or added selected levels to be always loaded", EUserInterfaceActionType::RadioButton, FInputGesture() );

		// change streaming method
		UI_COMMAND( SetStreamingMethod_Blueprint, "Change Blueprint Streaming Method", "Changes the streaming method for the selected levels to Blueprint streaming", EUserInterfaceActionType::Check, FInputGesture() );
		UI_COMMAND( SetStreamingMethod_AlwaysLoaded, "Change Streaming to Always Loaded", "Changes the streaming method for the selected levels to be always loaded", EUserInterfaceActionType::Check, FInputGesture() );

		//level selection
		UI_COMMAND( SelectAllLevels, "Select All Levels", "Selects all levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( DeselectAllLevels, "De-select All Levels", "De-selects all levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( InvertLevelSelection, "Invert Level Selection", "Inverts level selection", EUserInterfaceActionType::Button, FInputGesture() );
		
		//actors
		UI_COMMAND( SelectLevelActors, "Select Actors in Levels", "Sets actors in the selected Levels as the viewport's selection", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( DeselectLevelActors, "Deselect Actors in Levels", "Removes the Actors in the selected Levels from the viewport's existing selection", EUserInterfaceActionType::Button, FInputGesture() );

		//streaming volumes
		UI_COMMAND( AddStreamingLevelVolumes, "Add Streaming Volumes", "Adds the streaming volumes to the selected levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SetStreamingLevelVolumes, "Set Streaming Volumes", "Clears the streaming volumes associated with the selected levels and adds the selected streaming volumes", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( SelectStreamingVolumes, "Select Streaming Volumes", "Selects the streaming volumes associated with the selected levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( ClearStreamingVolumes, "Clear Streaming Volumes", "Removes all streaming volume associations with the selected levels", EUserInterfaceActionType::Button, FInputGesture() );

		//visibility
		UI_COMMAND( ShowSelectedLevels, "Show Selected Levels", "Toggles selected levels to a visible state in the viewports", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( HideSelectedLevels, "Hide Selected Levels", "Toggles selected levels to an invisible state in the viewports", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( ShowOnlySelectedLevels, "Show Only Selected Levels", "Toggles the selected levels to a visible state; toggles all other levels to an invisible state", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( ShowAllLevels, "Show All Levels", "Toggles all levels to a visible state in the viewports", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( HideAllLevels, "Hide All Levels", "Hides all levels to an invisible state in the viewports", EUserInterfaceActionType::Button, FInputGesture() );

		//lock
		UI_COMMAND( LockSelectedLevels, "Lock Selected Levels", "Locks selected levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( UnockSelectedLevels, "Unlock Selected Levels", "Unlocks selected levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( LockAllLevels, "Lock All Levels", "Locks all levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( UnockAllLevels, "Unlock All Levels", "Unlocks all levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( LockReadOnlyLevels, "Lock Read-Only Levels", "Locks all read-only levels", EUserInterfaceActionType::Button, FInputGesture() );
		UI_COMMAND( UnlockReadOnlyLevels, "Unlock Read-Only Levels", "Unlocks all read-only levels", EUserInterfaceActionType::Button, FInputGesture() );
	}
PRAGMA_ENABLE_OPTIMIZATION

public:

	/** makes the selected level the current level */
	TSharedPtr< FUICommandInfo > MakeLevelCurrent;

	/** Moves the selected actors to the selected level */
	TSharedPtr< FUICommandInfo > MoveActorsToSelected;

	/** Replaces selected invalid level with an already existing one, prompts for path */
	TSharedPtr< FUICommandInfo > FixUpInvalidReference;

	/** Removes the ULevelStreaming reference to the selected invalid levels */
	TSharedPtr< FUICommandInfo > RemoveInvalidReference;
	
	/** Opens the edit properties window for the selected levels */
	TSharedPtr< FUICommandInfo > EditProperties;

	/** Save selected levels; prompts for checkout if necessary */
	TSharedPtr< FUICommandInfo > SaveSelectedLevels;

	/** Check-Out selected levels from the SCC */
	TSharedPtr< FUICommandInfo > SCCCheckOut;

	/** Check-In selected levels from the SCC */
	TSharedPtr< FUICommandInfo > SCCCheckIn;

	/** Add selected levels to the SCC */
	TSharedPtr< FUICommandInfo > SCCOpenForAdd;

	/** Open a window to display the SCC history of the selected levels */
	TSharedPtr< FUICommandInfo > SCCHistory;

	/** Refresh the status of selected levels in SCC */
	TSharedPtr< FUICommandInfo > SCCRefresh;

	/** Diff selected levels against the version in the SCC depot */
	TSharedPtr< FUICommandInfo > SCCDiffAgainstDepot;

	/** Enable source control features */
	TSharedPtr< FUICommandInfo > SCCConnect;

	/** Migrate selected levels; copies levels and all their dependencies to another game */
	TSharedPtr< FUICommandInfo > MigrateSelectedLevels;

	/** Toggles whether the number of actors in each level is displayed */
	TSharedPtr< FUICommandInfo > DisplayActorCount;

	/** Toggles whether the Lightmass Size for each level is displayed */
	TSharedPtr< FUICommandInfo > DisplayLightmassSize;

	/** Toggles whether the File Size for each level is displayed */
	TSharedPtr< FUICommandInfo > DisplayFileSize;

	/** Toggles whether the Path information for each level is displayed */
	TSharedPtr< FUICommandInfo > DisplayPaths;

	/** Toggles whether the Editor Offset for each level is displayed */
	TSharedPtr< FUICommandInfo > DisplayEditorOffset;

	/** Creates a new empty level; prompts for save path */
	TSharedPtr< FUICommandInfo > CreateEmptyLevel;

	/** Adds an existing streaming level to the persistent; prompts for path */
	TSharedPtr< FUICommandInfo > AddExistingLevel;

	/** Creates a new empty level and moves the selected actors to it; prompts for save path */
	TSharedPtr< FUICommandInfo > AddSelectedActorsToNewLevel;

	/** Removes the selected levels from the persistent */
	TSharedPtr< FUICommandInfo > RemoveSelectedLevels;

	/** Merges the selected levels into a new level; prompts for save path; removes the levels that were merged */
	TSharedPtr< FUICommandInfo > MergeSelectedLevels;

	/** Sets the streaming method for new or added levels to Blueprint streaming */
	TSharedPtr< FUICommandInfo > SetAddStreamingMethod_Blueprint;

	/** Sets the streaming method for new or added levels to be always loaded */
	TSharedPtr< FUICommandInfo > SetAddStreamingMethod_AlwaysLoaded;

	/** Changes the streaming method for the selected levels to Blueprint streaming */
	TSharedPtr< FUICommandInfo > SetStreamingMethod_Blueprint;

	/** Changes the streaming method for the selected levels to be always loaded */
	TSharedPtr< FUICommandInfo > SetStreamingMethod_AlwaysLoaded;

	/** Selects all levels within the level browser */
	TSharedPtr< FUICommandInfo > SelectAllLevels;

	/** Deselects all levels within the level browser */
	TSharedPtr< FUICommandInfo > DeselectAllLevels;

	/** Inverts the level selection within the level browser */
	TSharedPtr< FUICommandInfo > InvertLevelSelection;


	/** Selects the actors in the selected Levels */
	TSharedPtr< FUICommandInfo > SelectLevelActors;

	/** Deselects the actors in the selected Levels */
	TSharedPtr< FUICommandInfo > DeselectLevelActors;

	/** Add the streaming volumes to the selected levels */
	TSharedPtr< FUICommandInfo > AddStreamingLevelVolumes;

	/** Sets the streaming volumes to the selected levels */
	TSharedPtr< FUICommandInfo > SetStreamingLevelVolumes;

	/** Selects the streaming volumes associated with the selected levels */
	TSharedPtr< FUICommandInfo > SelectStreamingVolumes;

	/** Removes all streaming volume associations with the selected levels */
	TSharedPtr< FUICommandInfo > ClearStreamingVolumes;


	/** Makes selected Levels visible */
	TSharedPtr< FUICommandInfo > ShowSelectedLevels;

	/** Makes selected Levels invisible */
	TSharedPtr< FUICommandInfo > HideSelectedLevels;

	/** Makes selected Levels visible; makes all others invisible */
	TSharedPtr< FUICommandInfo > ShowOnlySelectedLevels;

	/** Makes all Levels visible */
	TSharedPtr< FUICommandInfo > ShowAllLevels;

	/** Makes all Levels invisible */
	TSharedPtr< FUICommandInfo > HideAllLevels;


	/** Locks selected levels */
	TSharedPtr< FUICommandInfo > LockSelectedLevels;

	/** Unlocks selected levels */
	TSharedPtr< FUICommandInfo > UnockSelectedLevels;

	/** Locks all levels */
	TSharedPtr< FUICommandInfo > LockAllLevels;

	/** Unlocks selected levels */
	TSharedPtr< FUICommandInfo > UnockAllLevels;

	/** Locks all read-only levels */
	TSharedPtr< FUICommandInfo > LockReadOnlyLevels;

	/** Unlocks all read-only levels */
	TSharedPtr< FUICommandInfo > UnlockReadOnlyLevels;
};
