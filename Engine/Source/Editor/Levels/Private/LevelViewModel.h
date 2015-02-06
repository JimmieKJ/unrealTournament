// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "EditorUndoClient.h"
#include "Layers/Layer.h"

namespace ELevelsAction
{
	enum Type
	{
		/**	The specified ChangedLevel is a newly created ULevel, if ChangedLevel is invalid then multiple Levels were added */	
		Add,

		/**	
		 *	The specified ChangedLevel was just modified, if ChangedLevel is invalid then multiple Levels were modified. 
		 *  ChangedProperty specifies what field on the ULevel was changed, if NAME_None then multiple fields were changed 
		 */
		Modify,

		/**	A ULevel was deleted */
		Delete,

		/**	The specified ChangedLevel was just renamed */
		Rename,

		/**	A large amount of changes have occurred to a number of Levels. A full rebind will be required. */
		Reset,
	};
}

/**
 * The non-UI solution specific presentation logic for a single Level
 */
class FLevelViewModel : public TSharedFromThis< FLevelViewModel >, public FEditorUndoClient	
{

public:
	
	typedef IFilter< const TWeakObjectPtr< AActor >& > ActorFilter;

	/** FLevelViewModel destructor */
	virtual ~FLevelViewModel();

	/**  
	 *	Factory method which creates a new FLevelViewModel object
	 *
	 *	@param	InLevel				The Level wrap
	 *	@param	InLevelStreaming	The LevelStreaming wrap
	 *	@param	InWorldLevels		The Level management logic object
	 *	@param	InEditor			The UEditorEngine to use
	 */

	static TSharedRef< FLevelViewModel > Create(  const TWeakObjectPtr< class ULevel >& InLevel,
												  const TWeakObjectPtr< class ULevelStreaming >& InLevelStreaming, 
												  const TWeakObjectPtr< UEditorEngine >& InEditor )
	{
		TSharedRef< FLevelViewModel > NewLevel( new FLevelViewModel( InLevel, InLevelStreaming, InEditor ) );
		NewLevel->Initialize();

		return NewLevel;
	}

public:

	/** @return the StreamingLevelIndex */
	int GetStreamingLevelIndex() { return StreamingLevelIndex; }

	/** Refreshes the StreamingLevelIndex by checking or its position in its OwningWorld's Level array */
	void RefreshStreamingLevelIndex();

	/**	@return	The Level's display name as a FName */
	FName GetFName() const;

	/**	@return	The Level's display name as a FString 
	  * @param	bForceDisplayPath	Whether to include path for this level
	  * @param	bDisplayTags		Whether to include tags (ie: Persistent/Current/Dirty) */
	FString GetName(bool bForceDisplayPath=false, bool bDisplayTags=true) const;

	/**	@return	The Level's display name as a FText;
	  * wrapper function for GetName that simplifies function pointer for slate */
	FText GetDisplayName() const;

	/** @return The Level's Actor Count as a FText */
	FText GetActorCountString() const;

	/** @return the Level's Lightmass Size as a FText */
	FText GetLightmassSizeString() const;

	/** @return the Level's File Size as a FText */
	FText GetFileSizeString() const;

	/** @return whether the Level is dirty */
	bool IsDirty() const;

	/** @return whether the Level is the current Level */
	bool IsCurrent() const;

	/**	@return Whether the Level is the persistent level */
	bool IsPersistent() const;

	/** @return	True if this item represents a level, otherwise false */
	bool IsLevel() const;

	/** @return True if this has a valid ULevelStreaming reference */
	bool IsLevelStreaming() const;

	/**	@return Whether the Level is visible */
	bool IsVisible() const;

	/**	Toggles the Level's visibility */
	void ToggleVisibility();

	/** Sets the Level's visibility */
	void SetVisible(bool bVisible);

	/**	@return whether the Level is read-only */
	bool IsReadOnly() const;

	/**	@return whether the Level is locked */
	bool IsLocked() const;

	/**	@return whether the Level is valid */
	bool IsValid() const;

	/**	Toggles the Level's locked/unlocked state */
	void ToggleLock();

	/** Sets the Level's locked/unlocked state */
	void SetLocked(bool bLocked);

	/** Sets the Level's streaming class */
	void SetStreamingClass( UClass *LevelStreamingClass );

	/**	@return whether the Level has kismet */
	bool HasKismet() const;

	/** Opens the kismet for this level */
	void OpenKismet();


	/**	Makes this level the current level */
	void MakeLevelCurrent();


	/** Saves this level, prompting for source-control checkout if necessary */
	void Save();


	/** Gets the color for this level */
	FSlateColor GetColor() const;

	/** 
	 * Changes the color for this level.
	 * @param	InPickerParentWidget	The parent widget for the color picker window.
	 */
	void ChangeColor( const TSharedRef<class SWidget>& InPickerParentWidget );

	/**
	 *	Renames the Level to the specified name
	 *
	 *	@param	NewLevelName	The Levels new name
	 */
	void RenameTo( const FString& NewLevelName );

	/**
	 *	Returns whether the specified actors can be assigned to the Level
	 *
	 *	@param	Actors				The Actors to check if assignment is valid
	 *	@param	OutMessage [OUT]	A returned description explaining the boolean result
	 *	@return						if true then at least one actor can be assigned; 
	 *								If false, either Invalid actors were discovered or all actors are already assigned
	 */
	bool CanAssignActors(const TArray< TWeakObjectPtr<AActor> > Actors, FText& OutMessage) const;

	/**
	 *	Returns whether the specified actor can be assigned to the Level
	 *
	 *	@param	Actor				The Actor to check if assignment is valid
	 *	@param	OutMessage [OUT]	A returned description explaining the boolean result
	 *	@return						if true then at least one actor can be assigned; 
	 *								If false, either the Actor is Invalid or already assigned
	 */
	bool CanAssignActor(const TWeakObjectPtr<AActor> Actor, FText& OutMessage) const;
	
	/**
	 *	Appends all of the actors associated with this Level to the specified list
	 * 
	 *	@param	InActors	the list to append to
	 */
	void AppendActors( TArray< TWeakObjectPtr< AActor > >& InActors ) const;

	/**
	 *	Appends all of the actors associated with this Level to the specified list
	 * 
	 *	@param	InActors	the list to append to
	 */
	void AppendActorsOfSpecificType( TArray< TWeakObjectPtr< AActor > >& InActors, const TWeakObjectPtr< UClass >& Class );

	/**
	 *	Adds the specified actor to the Level
	 *
	 *	@param	Actor	the actor to add
	 */
	void AddActor( const TWeakObjectPtr<AActor>& Actor );

	/** 
	 *	Adds the specified actors to the Level
	 *
	 *	@param	Actors		The actors to add
	 *	@param	LevelName	The Level to add the actors to
	 */
	void AddActors( const TArray< TWeakObjectPtr<AActor> >& Actors );

	/** 
	 *	Removes the specified actors from the Level
	 *
	 *	@param	Actors		The actors to add
	 *	@param	LevelName	The Level to add the actors to
	 */
	void RemoveActors( const TArray< TWeakObjectPtr<AActor> >& Actors );

	/**
	 *	Removes the specified actor from the Level
	 *
	 *	@param	Actor	the actor to remove
	 */
	void RemoveActor( const TWeakObjectPtr<AActor>& Actor );

	/**
	 *	Selects in the Editor all the Actors assigned to the Level, based on the specified conditions.
	 *
	 *	@param	bSelect					if true actors will be selected; If false, actors will be deselected
	 *	@param	bNotify					if true the editor will be notified of the selection change; If false, the editor will not
	 *	@param	bSelectEvenIfHidden		if true actors that are hidden will be selected; If false, they will be skipped
	 *	@param	Filter					Only actors which pass the filters restrictions will be selected
	 */
	void SelectActors( bool bSelect, bool bNotify, bool bSelectEvenIfHidden, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL ) );

	/**	Selects the Actors assigned to the Level that are of a certain type */
	void SelectActorsOfSpecificType( const TWeakObjectPtr< UClass >& Class );
	
	/** Sets the ULevel this viewmodel should represent */
	void SetDataSource( const TWeakObjectPtr< ULevel >& InLevel );

	/** @return The ULevel this viewmodel represents */
	const TWeakObjectPtr< ULevel > GetLevel();

	/** @return The ULevelStreaming this viewmodel contains*/
	const TWeakObjectPtr< ULevelStreaming > GetLevelStreaming();

	/********************************************************************
	 * EVENTS
	 ********************************************************************/

	/** Broadcasts whenever the Level changes */
	DECLARE_EVENT( FLevelViewModel, FChangedEvent )
	FChangedEvent& OnChanged() { return ChangedEvent; }

	/** Updates cached value of level actors count */
	void UpdateLevelActorsCount();
private:

	/**
	 *	FLevelViewModel Constructor
	 *
	 *	@param	InLevel						The Level to represent
	 *	@param	InLevelStreaming			The LevelStreaming to represent
	 *	@param	InEditor					The UEditorEngine to use
	 */
	FLevelViewModel( const TWeakObjectPtr< class ULevel >& InLevel, 
					 const TWeakObjectPtr< class ULevelStreaming >& InLevelStreaming, 
					 const TWeakObjectPtr< UEditorEngine >& InEditor );

	/** Initializes the FLevelViewModel for use */
	void Initialize();

	/** Called when a change occurs regarding Levels */
	void OnLevelsChanged( const ELevelsAction::Type Action, const ULevel* ChangedLevel, const FName& ChangedProperty );

	/** Refreshes any cached information in the FLevelViewModel */
	void Refresh();

	/** Sets the Level dirty flag to not-dirty */
	void ClearDirtyFlag();

	/** Callback invoked when the color picker is cancelled */
	void OnColorPickerCancelled(FLinearColor Color);
	
	// Begin FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override { Refresh(); }
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

private:

	/** The Actor stats of the Level */
	TArray< FLayerActorStats > ActorStats;

	/** The UEditorEngine to use */
	const TWeakObjectPtr< UEditorEngine > Editor;

	/** The Level this object represents */
	TWeakObjectPtr< ULevel > Level;

	/** The LevelStreaming this object represents */
	TWeakObjectPtr< ULevelStreaming > LevelStreaming;

	/** The index of this level within its parent UWorld's StreamingLevels Array;  Used for manual sorting */
	int StreamingLevelIndex;

	/** Broadcasts whenever the Level changes */
	FChangedEvent ChangedEvent;

	/** Set to false if the color picker for changing level color was cancelled by the user */
	bool bColorPickerOK;
	
	/** This level actors count, excluding deleted actors */
	int32 LevelActorsCount;	
};