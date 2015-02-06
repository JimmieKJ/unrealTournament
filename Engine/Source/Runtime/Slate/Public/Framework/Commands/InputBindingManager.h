// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


typedef TMap<FName, TSharedPtr<FUICommandInfo>> FCommandInfoMap;
typedef TMap<FInputGesture, FName> FGestureMap;


/** Delegate for alerting subscribers the input manager records a user-defined gesture */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserDefinedGestureChanged, const FUICommandInfo& );


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
	 *								by the action so when the command's gesture is pressed, the action is executed
	 */
	void CreateInputCommand( const TSharedRef<FBindingContext>& InBindingContext, TSharedRef<FUICommandInfo> InUICommandInfo );

	/**
	 * Returns a command info that is has the same active gesture as the provided gesture and is in the same binding context or parent context
	 *
	 * @param	InBindingContext		The context in which the command is valid
	 * @param	InGesture				The gesture to match against commands
	 * @param	bCheckDefault			Whether or not to check the default gesture.  Will check the active gesture if false
	 * @return	A pointer to the command info which has the InGesture as its active gesture or null if one cannot be found
	 */
	const TSharedPtr<FUICommandInfo> GetCommandInfoFromInputGesture( const FName InBindingContext, const FInputGesture& InGesture, bool bCheckDefault ) const;

	/** 
	 * Finds the command in the provided context which uses the provided input gesture
	 * 
	 * @param InBindingContext	The binding context name
	 * @param InGesture			The gesture to check against when looking for commands
	 * @param bCheckDefault		Whether or not to check the default gesture of commands instead of active gestures
	 * @param OutActiveGestureIndex	The index into the commands active gesture array where the passed in gesture was matched.  INDEX_NONE if bCheckDefault is true or nothing was found.
	 */
	const TSharedPtr<FUICommandInfo> FindCommandInContext( const FName InBindingContext, const FInputGesture& InGesture, bool bCheckDefault ) const;

	/** 
	 * Finds the command in the provided context which has the provided name 
	 * 
	 * @param InBindingContext	The binding context name
	 * @param CommandName		The name of the command to find
	 */
	const TSharedPtr<FUICommandInfo> FindCommandInContext( const FName InBindingContext, const FName CommandName ) const;

	/**
	 * Called when the active gesture is changed on a command
	 *
	 * @param CommandInfo	The command that had the active gesture changed. 
	 */
	virtual void NotifyActiveGestureChanged( const FUICommandInfo& CommandInfo );
	
	/**
	 * Saves the user defined gestures to a json file
	 */
	void SaveInputBindings();

	/**
	 * Removes any user defined gestures
	 */
	void RemoveUserDefinedGestures();

	/**
	 * Returns all known command infos for a given binding context
	 *
	 * @param InBindingContext	The binding context to get command infos from
	 * @param OutCommandInfos	The list of command infos for the binding context
	 */
	void GetCommandInfosFromContext( const FName InBindingContext, TArray< TSharedPtr<FUICommandInfo> >& OutCommandInfos ) const;

	/** Registers a delegate to be called when a user-defined gesture is edited */
	FDelegateHandle RegisterUserDefinedGestureChanged(const FOnUserDefinedGestureChanged::FDelegate& Delegate)
	{
		return OnUserDefinedGestureChanged.Add(Delegate);
	}

	/** Unregisters a delegate to be called when a user-defined gesture is edited */
	DELEGATE_DEPRECATED("This UnregisterUserDefinedGestureChanged overload is deprecated - please remove delegates using the FDelegateHandle returned by the RegisterUserDefinedGestureChanged function.")
	void UnregisterUserDefinedGestureChanged(const FOnUserDefinedGestureChanged::FDelegate& Delegate)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS

		OnUserDefinedGestureChanged.Remove(Delegate);

		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Unregisters a delegate to be called when a user-defined gesture is edited */
	void UnregisterUserDefinedGestureChanged(FDelegateHandle DelegateHandle)
	{
		OnUserDefinedGestureChanged.Remove(DelegateHandle);
	}

private:

	/**
	 * Hidden default constructor.
	 */
	FInputBindingManager() { }

	/**
	 * Gets the user defined gesture (if any) from the provided command name
	 * 
	 * @param InBindingContext	The context in which the command is active
	 * @param InCommandName		The name of the command to get the gesture from
	 */
	bool GetUserDefinedGesture( const FName InBindingContext, const FName InCommandName, FInputGesture& OutUserDefinedGesture );

	/**
	 *	Checks a binding context for duplicate gestures 
	 */
	void CheckForDuplicateDefaultGestures( const FBindingContext& InBindingContext, TSharedPtr<FUICommandInfo> InCommandInfo ) const;

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

		/** Gesture to command info map */
		FGestureMap GestureToCommandInfoMap;

		/** The binding context for this entry*/
		TSharedPtr< FBindingContext > BindingContext;
	};

	/** A mapping of context name to the associated entry map */
	TMap< FName, FContextEntry > ContextMap;
	
	/** A mapping of contexts to their child contexts */
	TMultiMap< FName, FName > ParentToChildMap;

	/** User defined gesture overrides for commands */
	TSharedPtr< class FUserDefinedGestures > UserDefinedGestures;

	/** Delegate called when a user-defined gesture is edited */
	FOnUserDefinedGestureChanged OnUserDefinedGestureChanged;
};
