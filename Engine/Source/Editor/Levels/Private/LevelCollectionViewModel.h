// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Developer/AssetTools/Public/IAssetTypeActions.h"
#include "EditorUndoClient.h"

class FAssetData;
class ALevelStreamingVolume;

typedef TFilterCollection< const TSharedPtr< FLevelViewModel >& > LevelFilterCollection;
typedef IFilter< const TSharedPtr< FLevelViewModel >& > LevelFilter;

/** The non-UI solution specific presentation logic for a LevelsView */
class FLevelCollectionViewModel 
	: public TSharedFromThis< FLevelCollectionViewModel >
	, public FEditorUndoClient
	, public FTickableEditorObject
{

public:
	
	/** FLevelCollectionViewModel destructor */
	virtual ~FLevelCollectionViewModel();

	/**  
	 *	Factory method which creates a new FLevelCollectionViewModel object
	 *
	 *	@param	InWorldLevels	The Level management logic object
	 *	@param	InEditor		The UEditorEngine to use
	 */
	static TSharedRef< FLevelCollectionViewModel > Create( const TWeakObjectPtr< UEditorEngine >& InEditor )
	{
		TSharedRef< FLevelCollectionViewModel > LevelsView( new FLevelCollectionViewModel( InEditor ) );
		LevelsView->Initialize();

		return LevelsView;
	}

public:
	// Begin FTickableEditorObject Interface
	virtual bool IsTickable(void) const override { return true; }
	virtual void Tick( float DeltaTime ) override;
	virtual TStatId GetStatId() const override;
	// End of FTickableEditorObject

	/** Handler for when a world has been added. */
	void WorldAdded( UWorld* InWorld );	

	/** Handler for when a world has been destroyed. */
	void WorldDestroyed( UWorld* InWorld );

	/** Handler for when an actor was added to a level */
	void OnLevelActorAdded( AActor* InActor );	

	/** Handler for when an actor was removed from a level */
	void OnLevelActorDeleted( AActor* InActor );	

	/** Setup the handlers for a level changing. */
	void AddLevelChangeHandlers( UWorld* InWorld );

	/** Removed the handlers for a level changing. */
	void RemoveLevelChangeHandlers( UWorld* InWorld );

	/** @return Whether the current selection can be shifted */
	bool CanShiftSelection();

	/** Moves the level selection up or down in the list; used for re-ordering */
	void ShiftSelection( bool bUp );

	/** 
	 *	Adds a filter which restricts the Levels shown in the LevelsView
	 *	
	 *	@param	InFilter	The Filter to add
	 */
	void AddFilter( const TSharedRef< LevelFilter >& InFilter );

	/** 
	 *	Removes a filter which restricted the Levels shown in the LevelsView
	 *
	 *	@param	InFilter	The Filter to remove		
	 */
	void RemoveFilter( const TSharedRef< LevelFilter >& InFilter );

	/** @return	The list of ULevel objects to be displayed in the LevelsView */
	TArray< TSharedPtr< FLevelViewModel > >& GetLevels();

	/** @return	The selected ULevel objects in the LevelsView */
	const TArray< TSharedPtr< FLevelViewModel > >& GetSelectedLevels() const;

	/** @return Any selected ULevel objects in the LevelsView that are NULL */
	const TArray< TSharedPtr< FLevelViewModel > >& GetInvalidSelectedLevels() const;
	
	/**
	 *	Appends the names of the currently selected Levels to the provided array
	 *
	 * @param	OutSelectedLevelNames	The array to append the Level names to
	 */
	void GetSelectedLevelNames( OUT TArray< FName >& OutSelectedLevelNames ) const;

	/** 
	 *	Sets the specified array of ULevel objects as the currently selected Levels
	 *
	 *	@param	InSelectedLevels	The Levels to select
	 */
	void SetSelectedLevels( const TArray< TSharedPtr< FLevelViewModel > >& InSelectedLevels );

	/** Set the current selection to the specified Level names */
	void SetSelectedLevels( const TArray< FName >& LevelNames );

	/** Set the current selection to the specified Levels */
	void SetSelectedLevelsInWorld( const TArray< ULevel* >& Levels );

	/** Set the current selection to the specified Level */
	void SetSelectedLevel( const FName& LevelName );

	/** @return	The UICommandList supported by the LevelsView */
	const TSharedRef< FUICommandList > GetCommandList() const;
	
	/** true if the SCC Check-Out option is available */
	bool CanExecuteSCCCheckOut() const
	{
		return bCanExecuteSCCCheckOut;
	}

	/** true if the SCC Check-In option is available */
	bool CanExecuteSCCCheckIn() const
	{
		return bCanExecuteSCCCheckIn;
	}

	/** true if the SCC Mark for Add option is available */
	bool CanExecuteSCCOpenForAdd() const
	{
		return bCanExecuteSCCOpenForAdd;
	}

	/** true if Source Control options are generally available. */
	bool CanExecuteSCC() const
	{
		return bCanExecuteSCC;
	}

	/** Caches the variables for which SCC menu options are available */
	void CacheCanExecuteSourceControlVars();

	/********************************************************************
	 * EVENTS
	 ********************************************************************/

	/** Broadcasts whenever the number of Levels changes */
	DECLARE_EVENT_ThreeParams( FLevelCollectionViewModel, FOnLevelsChanged, const ELevelsAction::Type /*Action*/, const TWeakObjectPtr< ULevel >& /*ChangedLevel*/, const FName& /*ChangedProperty*/);
	FOnLevelsChanged& OnLevelsChanged() { return LevelsChanged; }

	/**	Broadcasts whenever the Level selection changes */
	DECLARE_EVENT( FLevelCollectionViewModel, FOnSelectionChanged );
	FOnSelectionChanged& OnSelectionChanged() { return SelectionChanged; }

	/** Broadcasts whenever the view settings for displaying Actor Count changes. */
	DECLARE_EVENT_OneParam( FLevelCollectionViewModel, FOnDisplayActorCountChanged, bool /*Enabled*/);
	FOnDisplayActorCountChanged& OnDisplayActorCountChanged() { return DisplayActorCountChanged; }

	/** Broadcasts whenever the view settings for displaying Lightmass Size changes. */
	DECLARE_EVENT_OneParam( FLevelCollectionViewModel, FOnDisplayLightmassSizeChanged, bool /*Enabled*/);
	FOnDisplayLightmassSizeChanged& OnDisplayLightmassSizeChanged() { return DisplayLightmassSizeChanged; }

	/** Broadcasts whenever the view settings for displaying File Size changes. */
	DECLARE_EVENT_OneParam( FLevelCollectionViewModel, FOnDisplayFileSizeChanged, bool /*Enabled*/);
	FOnDisplayFileSizeChanged& OnDisplayFileSizeChanged() { return DisplayFileSizeChanged; }

	/** Broadcasts whenever the view settings for displaying the Editor Offset changes. */
	DECLARE_EVENT_OneParam( FLevelCollectionViewModel, FOnDisplayEditorOffsetChanged, bool /*Enabled*/);
	FOnDisplayEditorOffsetChanged& OnDisplayEditorOffsetChanged() { return DisplayEditorOffsetChanged; }

private:

	/**  
	 *	FLevelCollectionViewModel Constructor
	 *
	 *	@param	InWorldLevels	The Level management logic object
	 *	@param	InEditor		The UEditorEngine to use
	 */
	FLevelCollectionViewModel( const TWeakObjectPtr< UEditorEngine >& InEditor );

	/** Initializes the LevelsView for use */
	void Initialize();

	/**	Refreshes any cached information */
	void Refresh();

	/** Refreshes cached selected levels */
	void RefreshSelected();

	/** Handles updating the viewmodel when one of its filters changes */
	void OnFilterChanged();

	/** Refreshes the Levels list */
	void OnLevelsChanged( const ELevelsAction::Type Action, const TWeakObjectPtr< ULevel >& ChangedLevel, const FName& ChangedProperty );

	/** Refreshes the Levels list */
	void OnResetLevels();

	/** Populates WorldLevels arrays */
	void PopulateLevelsList();

	/** Refreshes the sort index on all viewmodels that contain ULevels */
	void RefreshSortIndexes();

	/** Updates level actor count for all levels */
	void UpdateLevelActorsCount();

	/** Handles updating the internal viewmodels when new Levels are created */
	void OnLevelAdded( const TWeakObjectPtr< ULevel >& AddedLevel );

	/** Handles updating the internal viewmodels when Levels are deleted */
	void OnLevelDelete();

	/** Discards any viewmodels which are invalid */
	void DestructivelyPurgeInvalidViewModels();

	/** Creates ViewModels for all the Levels in the specified list */
	void CreateViewModels( const TArray< class ULevel* >& ActualLevels, const TArray< ULevelStreaming* >& InStreamedLevels );	

	/** Removes all streaming volume associations with the selected levels (without prompt) */
	void ClearStreamingLevelVolumes();

	/**
	 * Assembles a list of selected level streaming volumes.
	 *
	 * @param OutLevelStreamingVolumes		The list of selected level streaming volumes.
	 */
	void AssembleSelectedLevelStreamingVolumes(TArray<ALevelStreamingVolume*>& OutLevelStreamingVolumes);

	/** Adds the selected streaming level volumes to the selected levels. */
	void AddStreamingLevelVolumes();

	/** Rebuilds the list of filtered Levels */
	void RefreshFilteredLevels();

	/**	Sorts the filtered Levels list */
	void SortFilteredLevels();

	/** Binds all Level browser commands to delegates */
	void BindCommands();

	/** Appends the selected Level names to the specified array */
	void AppendSelectLevelNames( TArray< FName >& OutLevelNames ) const;

	// Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override { Refresh(); }
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

	/**
	 * Retrieves selected level packages and/or names.
	 *
	 * @param OutSelectedPackages		Filled with the selected packages.
	 * @param OutSelectedPackagesNames	Filled with the names of selected packages.
	 */
	void GetSelectedLevelPackages( TArray<UPackage*>* OutSelectedPackages, TArray<FString>* OutSelectedPackagesNames );
private:

	/** Toggles whether actor count is displayed in the levels list */
	void OnToggleDisplayActorCount();

	/** @return the state for the DisplayActorCount checkbox */
	bool GetDisplayActorCountState() const;

	/** Toggles whether Lightmass Size data is displayed in the levels list */
	void OnToggleLightmassSize();

	/** Toggles whether File Size data is displayed in the levels list */
	void OnToggleFileSize();

	/** Toggles whether Editor Offset data is displayed in the levels list */
	void OnToggleEditorOffset();

	/** @return the state for the DisplayLightmassSize checkbox */
	bool GetDisplayLightmassSizeState() const;

	/** @return the state for the DisplayFileSize checkbox */
	bool GetDisplayFileSizeState() const;

	/** Toggles whether Level paths are displayed in the levels list */
	void OnToggleDisplayPaths();

	/** @return the state for the DisplayPathsState checkbox */
	bool GetDisplayPathsState() const;

	/** @return the state for the DisplayEditorOffset checkbox */
	bool GetDisplayEditorOffsetState() const;

	/** Broadcast and update message if we are not currently refreshing. */
	void BroadcastUpdateIfNotInRefresh();
	
	/** @return	whether exactly one level is selected */
	bool IsOneLevelSelected() const;

	/** @return whether the currently selected level is unlocked */
	bool IsSelectedLevelUnlocked() const;

	/** @return	whether -at least- one level is selected */
	bool AreLevelsSelected() const;

	/** @return whether the currently selected levels are unlocked */
	bool AreSelectedLevelsUnlocked() const;

	/** @return whether the currently selected levels do not contain the persistent level */
	bool AreSelectedLevelsNotPersistent() const;

	/** @return whether the currently selected levels are unlocked and do not contain the persistent level */
	bool AreSelectedLevelsUnlockedAndNotPersistent() const;

	/** @return true if we have selected levels and they are all editable and visible, */
	bool AreAllSelectedLevelsUnlockedAndVisible() const;

	/** @return	whether -at least- one actor is selected */
	bool AreActorsSelected() const;
	
	/** @return whether properties can be edited for the selected levels */
	bool CanEditProperties() const;

	/** @return whether add or select streaming levels commands can be selected */
	bool CanAddOrSelectStreamingVolumes() const;

	/** Make this Level the Current Level */
	void MakeLevelCurrent_Executed();

	/** Moves the selected actors to this level */
	void MoveActorsToSelected_Executed();

	/** Adds an existing level; prompts for path */
	void FixupInvalidReference_Executed();

	/** Removes selected levels from world */
	void RemoveInvalidSelectedLevels_Executed();

	/** Opens the edit properties window for this level */
	void EditProperties_Executed();

	/** Saves selected levels */
	void SaveSelectedLevels_Executed();

	/** Check-Out selected levels from SCC */
	void OnSCCCheckOut();

	/** Mark for Add selected levels from SCC */
	void OnSCCOpenForAdd();

	/** Check-In selected levels from SCC */
	void OnSCCCheckIn();

	/** Shows the SCC History of selected levels */
	void OnSCCHistory();

	/** Refreshes the states selected levels from SCC */
	void OnSCCRefresh();

	/** Diffs selected levels from with those in the SCC depot */
	void OnSCCDiffAgainstDepot();

	/** Enable source control features */
	void OnSCCConnect() const;

	/** Migrate selected levels */
	void MigrateSelectedLevels_Executed();

	/** Creates a new empty Level; prompts for level save location */
	void CreateEmptyLevel_Executed();

	/** Calls AddExistingLevel which adds an existing level; prompts for path */
	void AddExistingLevel_Executed();
	
	/** Adds an existing level; prompts for path. If bRemoveInvalidSelectedLevelsAfter is true, any invalid levels are removed after */
	void AddExistingLevel(bool bRemoveInvalidSelectedLevelsAfter = false);

	/** Handler for when a level is selected after invoking AddExistingLevel */
	void HandleAddExistingLevelSelected(const TArray<FAssetData>& SelectedAssets, bool bRemoveInvalidSelectedLevelsAfter);

	/** Handler for when the dialog is cancelled after invoking AddExistingLevel */
	void HandleAddExistingLevelCancelled();

	/** Add Selected Actors to New Level; prompts for level save location */
	void AddSelectedActorsToNewLevel_Executed();

	/** Removes selected levels from world */
	void RemoveSelectedLevels_Executed();

	/** Merges selected levels into a new level; prompts for level save location */
	void MergeSelectedLevels_Executed();

	/** Changes the streaming method for new or added levels. */
	void SetAddedLevelStreamingClass_Executed(UClass* InClass);

	/** Checks if the passed in streaming method is checked */
	bool IsNewStreamingMethodChecked(UClass* InClass) const;

	/** Checks if the passed in streaming method is the current streaming method */
	bool IsStreamingMethodChecked(UClass* InClass) const;

	/** Changes the streaming method for the selected levels. */
	void SetStreamingLevelsClass_Executed(UClass* InClass);

	/** Selects all levels in the collection view model */
	void SelectAllLevels_Executed();

	/** De-selects all levels in the collection view model */
	void DeselectAllLevels_Executed();

	/** Inverts level selection in the collection view model */
	void InvertSelection_Executed();


	/** Adds the Actors in the selected Levels from the viewport's existing selection */
	void SelectActors_Executed();

	/** Removes the Actors in the selected Levels from the viewport's existing selection */
	void DeselectActors_Executed();

	/** Selects the streaming volumes associated with the selected levels */
	void SelectStreamingVolumes_Executed();

	/** Sets the streaming volumes to the selected levels. */
	void SetStreamingLevelVolumes_Executed();

	/** Adds the streaming volumes to the selected levels. */
	void AddStreamingLevelVolumes_Executed();

	/** Removes all streaming volume associations with the selected levels (with yes/no prompt) */
	void ClearStreamingVolumes_Executed();


	/** Toggles selected levels to a visible state in the viewports */
	void ShowSelectedLevels_Executed();

	/** Toggles selected levels to an invisible state in the viewports */
	void HideSelectedLevels_Executed();

	/** Toggles the selected levels to a visible state; toggles all other levels to an invisible state */
	void ShowOnlySelectedLevels_Executed();

	/** Toggles all levels to a visible state in the viewports */
	void ShowAllLevels_Executed();

	/** Hides all levels to an invisible state in the viewports */
	void HideAllLevels_Executed();


	/** Locks selected levels */
	void LockSelectedLevels_Executed();

	/** Unlocks selected levels */
	void UnockSelectedLevels_Executed();

	/** Locks all levels */
	void LockAllLevels_Executed();

	/** Unlocks all levels */
	void UnockAllLevels_Executed();
	
	/** Create the local arrays used to populate the browser window */
	void CreateLocalArrays();

	/** Set the current world that the browser will get levels from. */
	void SetCurrentWorld( UWorld* InWorld );

	/** Sets the visibility for the provided levels */
	void SetVisible(TArray< TSharedPtr< FLevelViewModel > >& LevelViewModels, bool bVisible );

	/** Toggle all read-only levels */
	void ToggleReadOnlyLevels_Executed();
	
	/** @return whether moving the selected actors to the selected level is a valid action */
	bool IsValidMoveActorsToLevel();

	/** delegate used to pickup when the selection has changed */
	void OnActorSelectionChanged(UObject* obj);

	/** Sets a flag to re-cache whether the selected actors move to the selected level is valid */
	void OnActorOrLevelSelectionChanged();
private:

	/** true if the LevelsView is in the middle of refreshing */
	bool bIsRefreshing;

	/** Whether we have pending request to update cached actors count for all levels */
	bool bPendingUpdateActorsCount;	

	/** The collection of filters used to restrict the Levels shown in the LevelsView */
	const TSharedRef< LevelFilterCollection > Filters;

	/** All Levels shown in the LevelsView */
	TArray< TSharedPtr< FLevelViewModel > > FilteredLevelViewModels;

	/** All Levels managed by the LevelsView */
	TArray< TSharedPtr< FLevelViewModel > > AllLevelViewModels;

	/** Currently selected Levels */
	TArray< TSharedPtr< FLevelViewModel > > SelectedLevels;

	/** Currently selected NULL Levels */
	TArray< TSharedPtr< FLevelViewModel > > InvalidSelectedLevels;

	/** The list of commands with bound delegates for the Level browser */
	const TSharedRef< FUICommandList > CommandList;

	/** The Level management logic object */
	TArray< class ULevel* > WorldLevels;

	/** The UEditorEngine to use */
	const TWeakObjectPtr< UEditorEngine > Editor;

	/**	Broadcasts whenever one or more Levels changes */
	FOnLevelsChanged LevelsChanged;

	/**	Broadcasts whenever the currently selected Levels changes */
	FOnSelectionChanged SelectionChanged;

	/** Broadcasts whenever the Lightmass Size display is toggled. */
	FOnDisplayLightmassSizeChanged DisplayLightmassSizeChanged;

	/** Broadcasts whenever the File Size display is toggled. */
	FOnDisplayFileSizeChanged DisplayFileSizeChanged;

	/** Broadcasts whenever the Actor Count display is toggled. */
	FOnDisplayActorCountChanged DisplayActorCountChanged;

	/** Broadcasts whenever the Editor Offset display is toggled. */
	FOnDisplayEditorOffsetChanged DisplayEditorOffsetChanged;

	/** The world to which this level collection relates */
	TWeakObjectPtr<UWorld> CurrentWorld;

	/** The current class to set new or added levels streaming method to. */
	UClass* AddedLevelStreamingClass;

	/** true if the SCC Check-Out option is available */
	bool bCanExecuteSCCCheckOut;

	/** true if the SCC Check-In option is available */
	bool bCanExecuteSCCOpenForAdd;

	/** true if the SCC Mark for Add option is available */
	bool bCanExecuteSCCCheckIn;

	/** true if Source Control options are generally available. */
	bool bCanExecuteSCC;
	
	/** Flag for whether the selection of levels or actors has changed */
	bool bSelectionHasChanged;

	/** Boolean indicating whether the asset dialog is currently open */
	bool bAssetDialogOpen;
};


