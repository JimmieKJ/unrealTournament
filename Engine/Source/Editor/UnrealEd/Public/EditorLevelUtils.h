// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorLevelUtils.h: Editor-specific level management routines
=============================================================================*/

#pragma once

#ifndef __EditorLevelUtils_h__
#define __EditorLevelUtils_h__

DECLARE_LOG_CATEGORY_EXTERN(LogLevelTools, Warning, All);


namespace EditorLevelUtils
{
	
	/**
	 * Moves the specified list of actors to the specified level
	 *
	 * @param	ActorsToMove		List of actors to move
	 * @param	DestLevelStreaming	The level streaming object associated with the destination level
	 * @param	OutNumMovedActors	The number of actors that were successfully moved to the new level
	 */
	void MovesActorsToLevel( TArray< AActor* >& ActorsToMove, ULevelStreaming* DestLevelStreaming, int32& OutNumMovedActors );

	/**
	 * Adds the named level packages to the world.  Does nothing if all the levels already exist in the world.
	 *
	 * @param	InWorld				World in which to add the level
	 * @param	LevelPackageName	The base filename of the level package to add.
	 * @param	LevelStreamingClass	The streaming class type instead to use for the level.
	 *
	 * @return								The new level, or NULL if the level couldn't added.
	 */
	UNREALED_API ULevel* AddLevelsToWorld(UWorld* InWorld, const TArray<FString>& LevelPackageNames, UClass* LevelStreamingClass);


	/**
	 * Adds the named level package to the world.  Does nothing if the level already exists in the world.
	 *
	 * @param	InWorld				World in which to add the level
	 * @param	LevelPackageName	The base filename of the level package to add.
	 * @param	LevelStreamingClass	The streaming class type instead to use for the level.
	 *
	 * @return								The new level, or NULL if the level couldn't added.
	 */
	UNREALED_API ULevel* AddLevelToWorld(UWorld* InWorld, const TCHAR* LevelPackageName, UClass* LevelStreamingClass);

	/** Sets the LevelStreamingClass for the specified Level 
	  * @param	InLevel				The level for which to change the streaming class
	  * @param	LevelStreamingClass	The desired streaming class
	  *
	  * @return	The new streaming level object
	  */
	UNREALED_API ULevelStreaming* SetStreamingClassForLevel(ULevelStreaming* InLevel, UClass* LevelStreamingClass);

	/**
	 * Removes the specified level from the world.  Refreshes.
	 *
	 * @return	true	If a level was removed.
	 */
	UNREALED_API bool RemoveLevelFromWorld(ULevel* InLevel);

	/**
	 * Removes the specified LevelStreaming from the world, and Refreshes.
	 * Used to Clean up references of missing levels.
	 *
	 * @return	true	If a level was removed.
	 */
	UNREALED_API bool RemoveInvalidLevelFromWorld(ULevelStreaming* InLevelStreaming);

	/**
	 * Creates a new streaming level.
	 *
	 * @param	InWorld								World in which to create the level
	 * @param	bMoveSelectedActorsIntoNewLevel		If true, move any selected actors into the new level.
	 * @param	LevelStreamingClass					The streaming class type instead to use for the level.
	 * @param	DefaultFilename						Optional file name for level.  If empty, the user will be prompted during the save process.
	 * 
	 * @return	Returns the newly created level, or NULL on failure
	 */
	UNREALED_API ULevel* CreateNewLevel(UWorld* InWorld, bool bMoveSelectedActorsIntoNewLevel, UClass* LevelStreamingClass, const FString& DefaultFilename = TEXT( "" ) );


	/**
	 * Sets a level's visibility in the editor.
	 *
	 * @param	Level					The level to modify.
	 * @param	bShouldBeVisible		The level's new visibility state.
	 * @param	bForceLayersVisible		If true and the level is visible, force the level's layers to be visible.
	 */
	UNREALED_API void SetLevelVisibility(ULevel* Level, bool bShouldBeVisible, bool bForceLayersVisible);
	
	/**
	 * Returns a class that represents the required streaming type (User selected via a dialog)
	 *
	 * @param	LevelPackageName		The name of the level (its displayed in the dialog)
	 *
	 * @returns	The streaming class or NULL if the user clicked cancel
	 */
	UClass* SelectLevelStreamType( const TCHAR* LevelPackageName );

	/**
	 * Removes a level from the world.  Returns true if the level was removed successfully.
	 *
	 * @param	Level		The level to remove from the world.
	 * @return				true if the level was removed successfully, false otherwise.
	 */
	bool PrivateRemoveLevelFromWorld( ULevel* Level );

	/** 
	 * Completely removes the level from the world, unloads its package and forces garbage collection.
	 *
	 * @note: This function doesn't remove the associated streaming level.
	 *
	 * @param	InLevel			A non-NULL, non-Persistent Level that will be destroyed.
	 * @return					true if the level was removed.
	 */
	bool EditorDestroyLevel( ULevel* InLevel );
	
	/** 
	 * Deselects all BSP surfaces in this level 
	 *
	 * @param InLevel		The level to deselect the surfaces of.
	 *
	 */	
	UNREALED_API void DeselectAllSurfacesInLevel(ULevel* InLevel);

	/**
	 * Makes the specified level current.
	 *
	 * @return	true	If a level was removed.
	 */
	UNREALED_API void MakeLevelCurrent(ULevel* InLevel);

	/**
	 * Assembles the set of all referenced worlds.
	 *
	 * @param	InWorld				World containing streaming levels
	 * @param	Worlds				[out] The set of referenced worlds.
	 * @param	bIncludeInWorld		If true, include the InWorld in the output list.
	 * @param	bOnlyEditorVisible	If true, only sub-levels that should be visible in-editor are included
	 */
	UNREALED_API void GetWorlds(UWorld* InWorld, TArray<UWorld*>& OutWorlds, bool bIncludeInWorld, bool bOnlyEditorVisible = false);

}




#endif	// __EditorLevelUtils_h__
