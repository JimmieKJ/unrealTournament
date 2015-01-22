// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Delegate type for determining whether a session can be selected.
 *
 * The first parameter is the session to be selected (or NULL if the selection is being cleared).
 * The second parameter is a Boolean out parameter allowing the callee to indicate whether the selection can take place (true by default).
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCanSelectSession, const ISessionInfoPtr&, bool&)

/**
 * Delegate type for instance selection changes.
 *
 * The first parameter is the newly selected session (or NULL if none was selected).
 */
DECLARE_MULTICAST_DELEGATE(FOnSessionInstanceSelectionChanged)

/**
 * Delegate type for session selection changes.
 *
 * The first parameter is the newly selected session (or NULL if none was selected).
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectedSessionChanged, const ISessionInfoPtr&)


/**
 * Interface for the session manager.
 */
class ISessionManager
{
public:

	/**
	 * Adds an owner whose sessions we are interested in
	 *
	 * @param InOwner Session owner we want to view sessions from
	 */
	virtual void AddOwner( const FString& InOwner ) = 0;

	/**
	  * Gets the collection of currently selected engine instances.
	  *
	  * @param OutInstances Will hold the selected instances.
	  */
	virtual void GetSelectedInstances( TArray<ISessionInstanceInfoPtr>& OutInstances) const = 0;

	/**
	 * Get the selected session - as chosen in the session browser
	 *
	 * @return The session ID selected
	 */
	virtual const ISessionInfoPtr& GetSelectedSession() const = 0;

	/**
	 * Gets the list of all discovered sessions.
	 *
	 * @param OutSessions Will hold the collection of sessions.
	 */
	virtual void GetSessions( TArray<ISessionInfoPtr>& OutSessions ) const = 0;

	/**
	 * Checks whether the given instance is currently selected.
	 *
	 * @return true if the instance is selected, false otherwise.
	 */
	virtual bool IsInstanceSelected( const ISessionInstanceInfoRef& Instance ) const = 0;

	/**
	 * Removes an owner whose sessions we are no longer interested in
	 *
	 * @param InOwner Session owner we want to remove
	 */
	virtual void RemoveOwner( const FString& InOwner ) = 0;

	/**
	 * Selects the specified session.
	 *
	 * @param Session The session to the select (can be NULL to select none).
	 * @return true if the session was selected, false otherwise.
	 */
	virtual bool SelectSession( const ISessionInfoPtr& Session ) = 0;

	/**
	 * Marks the specified item as selected or unselected.
	 *
	 * @param Instance The instance to mark.
	 * @param Selected Whether the instance should be selected (true) or unselected (false).
	 * @return true if the instance was selected, false otherwise.
	 */
	virtual bool SetInstanceSelected( const ISessionInstanceInfoPtr& Instance, bool Selected ) = 0;

public:

	/**
	 * Returns a delegate that is executed before a session is being selected.
	 *
	 * @return The delegate.
	 */
	virtual FOnCanSelectSession& OnCanSelectSession() = 0;

	/**
	 * Returns a delegate that is executed when an instance changes its selection state.
	 *
	 * @return The delegate.
	 */
	virtual FOnSessionInstanceSelectionChanged& OnInstanceSelectionChanged() = 0;

	/**
	 * Returns a delegate that is executed when the selected session received a log message from one of its instances.
	 *
	 * @return The delegate.
	 */
	virtual FOnSessionLogReceived& OnLogReceived() = 0;

	/**
	 * Returns a delegate that is executed when the selected session changed.
	 *
	 * @return The delegate.
	 */
	virtual FOnSelectedSessionChanged& OnSelectedSessionChanged() = 0;

	/**
	 * Returns a delegate that is executed when the list of sessions has changed.
	 *
	 * @return The delegate.
	 */
	virtual FSimpleMulticastDelegate& OnSessionsUpdated() = 0;

	/**
	 * Returns a delegate that is executed when a session instance is updated.
	 *
	 * @return The delegate.
	 */
	virtual FSimpleMulticastDelegate& OnSessionInstanceUpdated() = 0;

public:

	/** Virtual destructor. */
	virtual ~ISessionManager() { }
};


/** Type definition for shared pointers to instances of ISessionManager. */
typedef TSharedPtr<ISessionManager> ISessionManagerPtr;

/** Type definition for shared references to instances of ISessionManager. */
typedef TSharedRef<ISessionManager> ISessionManagerRef;
