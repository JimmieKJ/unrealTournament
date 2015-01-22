// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Defines FExecuteAction delegate interface */
DECLARE_DELEGATE(FExecuteAction);

/** Defines FCanExecuteAction delegate interface.  Returns true when an action is able to execute. */
DECLARE_DELEGATE_RetVal(bool, FCanExecuteAction);

/** Defines FIsActionChecked delegate interface.  Returns true if the action is currently toggled on. */
DECLARE_DELEGATE_RetVal(bool, FIsActionChecked);

/** Defines FIsActionButtonVisible delegate interface.  Returns true when UI buttons associated with the action should be visible. */
DECLARE_DELEGATE_RetVal(bool, FIsActionButtonVisible);


/**
 * Implements an UI action.
 */
struct FUIAction
{
	/** Holds a delegate that is executed when this action is activated. */
	FExecuteAction ExecuteAction;

	/** Holds a delegate that is executed when determining whether this action can execute. */
	FCanExecuteAction CanExecuteAction;

	/** Holds a delegate that is executed when determining whether this action is checked. */
	FIsActionChecked IsCheckedDelegate;

	/** Holds a delegate that is executed when determining whether this action is visible. */
	FIsActionButtonVisible IsActionVisibleDelegate;

public:

	/** Default constructor. */
	FUIAction( ) { }

	/**
	 * Constructor that takes delegates to initialize the action with
	 *
	 * @param	ExecuteAction		The delegate to call when the action should be executed
	 */
	FUIAction( FExecuteAction InitExecuteAction )
		: ExecuteAction(InitExecuteAction)
		, CanExecuteAction(FCanExecuteAction::CreateRaw(&FSlateApplication::Get(), &FSlateApplication::IsNormalExecution))
		, IsCheckedDelegate()
		, IsActionVisibleDelegate()
	{ }

	/**
	 * Constructor that takes delegates to initialize the action with
	 *
	 * @param	ExecuteAction		The delegate to call when the action should be executed
	 * @param	CanExecuteAction	The delegate to call to see if the action can be executed
	 */
	FUIAction( FExecuteAction InitExecuteAction, FCanExecuteAction InitCanExecuteAction )
		: ExecuteAction(InitExecuteAction )
		, CanExecuteAction(InitCanExecuteAction)
		, IsCheckedDelegate()
		, IsActionVisibleDelegate()
	{ }

	/**
	 * Constructor that takes delegates to initialize the action with
	 *
	 * @param	ExecuteAction		The delegate to call when the action should be executed
	 * @param	CanExecuteAction	The delegate to call to see if the action can be executed
	 * @param	IsCheckedDelegate	The delegate to call to see if the action should appear checked when visualized
	 */
	FUIAction( FExecuteAction InitExecuteAction, FCanExecuteAction InitCanExecuteAction, FIsActionChecked InitIsCheckedDelegate )
		: ExecuteAction(InitExecuteAction)
		, CanExecuteAction(InitCanExecuteAction)
		, IsCheckedDelegate(InitIsCheckedDelegate)
		, IsActionVisibleDelegate()
	{ }

	/**
	 * Constructor that takes delegates to initialize the action with
	 *
	 * @param	ExecuteAction			The delegate to call when the action should be executed
	 * @param	CanExecuteAction		The delegate to call to see if the action can be executed
	 * @param	IsCheckedDelegate		The delegate to call to see if the action should appear checked when visualized
	 * @param	IsActionVisibleDelegate	The delegate to call to see if the action should be visible
	 */
	FUIAction( FExecuteAction InitExecuteAction, FCanExecuteAction InitCanExecuteAction, FIsActionChecked InitIsCheckedDelegate, FIsActionButtonVisible InitIsActionVisibleDelegate )
		: ExecuteAction(InitExecuteAction)
		, CanExecuteAction(InitCanExecuteAction)
		, IsCheckedDelegate(InitIsCheckedDelegate)
		, IsActionVisibleDelegate(InitIsActionVisibleDelegate)
	{ }

public:

	/**
	 * Compares this UI action to another for equality.
	 *
	 * @param Other The other action to compare to.
	 * @return true if both actions are equal, false otherwise.
	 */
	bool operator==( const FUIAction& Other ) const
	{
		return
			(ExecuteAction == Other.ExecuteAction) &&
			(CanExecuteAction == Other.CanExecuteAction) &&
			(IsCheckedDelegate == Other.IsCheckedDelegate) &&
			(IsActionVisibleDelegate == Other.IsActionVisibleDelegate);
	}

public:

	/**
	 * Checks to see if its currently safe to execute this action.
	 *
	 * @return	True if this action can be executed and we should reflect that state visually in the UI
	 */
	bool CanExecute( ) const
	{
		// Fire the 'can execute' delegate if we have one, otherwise always return true
		return CanExecuteAction.IsBound() ? CanExecuteAction.Execute() : true;
	}

	/**
	 * Executes this action
	 */
	bool Execute( ) const
	{
		// It's up to the programmer to ensure that the action is still valid by the time the user clicks on the
		// button.  Otherwise the user won't know why the action didn't take place!
		if( CanExecute() )
		{
			return ExecuteAction.ExecuteIfBound();
		}

		return false;
	}

	/**
	 * Queries the checked state for this action.  This is only valid for actions that are toggleable!
	 *
	 * @return	Checked state for this action
	 */
	bool IsChecked( ) const
	{
		if (IsCheckedDelegate.IsBound())
		{
			const bool bIsChecked = IsCheckedDelegate.Execute();

			if (bIsChecked)
			{
				return true;
			}
		}

		return false;
	}

	/**
	 * Checks whether this action's execution delegate is bound.
	 *
	 * @return true if the delegate is bound, false otherwise.
	 */
	bool IsBound( ) const
	{
		return ExecuteAction.IsBound();
	}

	/**
	 * Queries the visibility for this action.
	 *
	 * @return	Visibility state for this action
	 */
	EVisibility IsVisible( ) const
	{
		if (IsActionVisibleDelegate.IsBound())
		{
			const bool bIsVisible = IsActionVisibleDelegate.Execute();
			return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
		}

		return EVisibility::Visible;
	}
};
