// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


typedef TMap<FName, TSharedPtr<FUICommandInfo>> FCommandInfoMap;
typedef TMap<FInputChord, FName> FChordMap;
DEPRECATED(4.8, "Use FChordMap instead of FGestureMap")
typedef FChordMap FGestureMap;


/** Delegate for alerting subscribers the input manager records a user-defined chord */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserDefinedChordChanged, const FUICommandInfo& );

DEPRECATED(4.8, "Use FOnUserDefinedChordChanged instead of FOnUserDefinedGestureChanged")
typedef FOnUserDefinedChordChanged FOnUserDefinedGestureChanged;

/**
 * Manager responsible for creating and processing input bindings.                  
 */
class SLATE_API FInputBindingManager
{
public:

	/**
	 * @return The instance of this manager                   
	 */
	static FInputBindingManager& Get();

	/**
	 * Returns a list of all known input contexts
	 *
	 * @param OutInputContexts	The generated list of contexts
	 * @return A list of all known input contexts                   
	 */
	void GetKnownInputContexts( TArray< TSharedPtr<FBindingContext> >& OutInputContexts ) const;

	/**
	 * Look up a binding context by name.
	 */
	TSharedPtr<FBindingContext> GetContextByName( const FName& InContextName );

	/**
	 * Remove the context with this name
	 */
	void RemoveContextByName( const FName& InContextName );

	/**
	 * Creates an input command from the specified user interface action
	 * 
	 * @param InBindingContext		The context where the command is valid
	 * @param InUserInterfaceAction	The user interface action to create the input command from
	 *								Will automatically bind the user interface action to the command specified 
	 *								by the action so when the command's chord is pressed, the action is executed
	 */
	void CreateInputCommand( const TSharedRef<FBindingContext>& InBindingContext, TSharedRef<FUICommandInfo> InUICommandInfo );

	/**
	 * Returns a command info that is has the same active chord as the provided chord and is in the same binding context or parent context
	 *
	 * @param	InBindingContext		The context in which the command is valid
	 * @param	InChord					The chord to match against commands
	 * @param	bCheckDefault			Whether or not to check the default chord.  Will check the active chord if false
	 * @return	A pointer to the command info which has the InChord as its active chord or null if one cannot be found
	 */
	DEPRECATED(4.8, "Use GetCommandInfoFromInputChord instead of GetCommandInfoFromInputGesture")
	const TSharedPtr<FUICommandInfo> GetCommandInfoFromInputGesture( const FName InBindingContext, const FInputChord& InGesture, bool bCheckDefault ) const;
	const TSharedPtr<FUICommandInfo> GetCommandInfoFromInputChord( const FName InBindingContext, const FInputChord& InChord, bool bCheckDefault ) const;

	/** 
	 * Finds the command in the provided context which uses the provided input chord
	 * 
	 * @param InBindingContext	The binding context name
	 * @param InChord			The chord to check against when looking for commands
	 * @param bCheckDefault		Whether or not to check the default chord of commands instead of active chords
	 * @param OutActiveChordIndex	The index into the commands active chord array where the passed in chord was matched.  INDEX_NONE if bCheckDefault is true or nothing was found.
	 */
	const TSharedPtr<FUICommandInfo> FindCommandInContext( const FName InBindingContext, const FInputChord& InChord, bool bCheckDefault ) const;

	/** 
	 * Finds the command in the provided context which has the provided name 
	 * 
	 * @param InBindingContext	The binding context name
	 * @param CommandName		The name of the command to find
	 */
	const TSharedPtr<FUICommandInfo> FindCommandInContext( const FName InBindingContext, const FName CommandName ) const;

	/**
	 * Called when the active chord is changed on a command
	 *
	 * @param CommandInfo	The command that had the active chord changed. 
	 */
	DEPRECATED(4.8, "Use NotifyActiveChordChanged instead of NotifyActiveGestureChanged")
	void NotifyActiveGestureChanged( const FUICommandInfo& CommandInfo );
	virtual void NotifyActiveChordChanged( const FUICommandInfo& CommandInfo );
	
	/**
	 * Saves the user defined chords to a json file
	 */
	void SaveInputBindings();

	/**
	 * Removes any user defined chords
	 */
	DEPRECATED(4.8, "Use RemoveUserDefinedChords instead of RemoveUserDefinedGestures")
	void RemoveUserDefinedGestures() { RemoveUserDefinedChords(); }
	void RemoveUserDefinedChords();

	/**
	 * Returns all known command infos for a given binding context
	 *
	 * @param InBindingContext	The binding context to get command infos from
	 * @param OutCommandInfos	The list of command infos for the binding context
	 */
	void GetCommandInfosFromContext( const FName InBindingContext, TArray< TSharedPtr<FUICommandInfo> >& OutCommandInfos ) const;

	/** Registers a delegate to be called when a user-defined chord is edited */
	DEPRECATED(4.8, "Use RegisterUserDefinedChordChanged instead of RegisterUserDefinedGestureChanged")
	FDelegateHandle RegisterUserDefinedGestureChanged(const FOnUserDefinedChordChanged::FDelegate& Delegate)
	{
		return RegisterUserDefinedChordChanged(Delegate);
	}
	FDelegateHandle RegisterUserDefinedChordChanged(const FOnUserDefinedChordChanged::FDelegate& Delegate)
	{
		return OnUserDefinedChordChanged.Add(Delegate);
	}

	/** Unregisters a delegate to be called when a user-defined chord is edited */
	DEPRECATED(4.8, "Use RegisterUserDefinedChordChanged instead of RegisterUserDefinedGestureChanged")
	void UnregisterUserDefinedGestureChanged(FDelegateHandle DelegateHandle)
	{
		UnregisterUserDefinedChordChanged(DelegateHandle);
	}
	void UnregisterUserDefinedChordChanged(FDelegateHandle DelegateHandle)
	{
		OnUserDefinedChordChanged.Remove(DelegateHandle);
	}

private:

	/**
	 * Hidden default constructor.
	 */
	FInputBindingManager() { }

	/**
	 * Gets the user defined chord (if any) from the provided command name
	 * 
	 * @param InBindingContext	The context in which the command is active
	 * @param InCommandName		The name of the command to get the chord from
	 */
	bool GetUserDefinedChord( const FName InBindingContext, const FName InCommandName, FInputChord& OutUserDefinedChord );

	/**
	 *	Checks a binding context for duplicate chords 
	 */
	void CheckForDuplicateDefaultChords( const FBindingContext& InBindingContext, TSharedPtr<FUICommandInfo> InCommandInfo ) const;

	/**
	 * Recursively finds all child contexts of the provided binding context.  
	 *
	 * @param InBindingContext	The binding context to search
	 * @param AllChildren		All the children of InBindingContext. InBindingContext is the first element in AllChildren
	 */
	void GetAllChildContexts( const FName InBindingContext, TArray<FName>& AllChildren ) const;

private:

	struct FContextEntry
	{
		/** A list of commands associated with the context */
		FCommandInfoMap CommandInfoMap;

		/** Chord to command info map */
		FChordMap ChordToCommandInfoMap;

		/** The binding context for this entry*/
		TSharedPtr< FBindingContext > BindingContext;
	};

	/** A mapping of context name to the associated entry map */
	TMap< FName, FContextEntry > ContextMap;
	
	/** A mapping of contexts to their child contexts */
	TMultiMap< FName, FName > ParentToChildMap;

	/** User defined chord overrides for commands */
	TSharedPtr< class FUserDefinedChords > UserDefinedChords;

	/** Delegate called when a user-defined chord is edited */
	FOnUserDefinedChordChanged OnUserDefinedChordChanged;
};
