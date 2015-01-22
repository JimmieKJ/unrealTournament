// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a filter for the message history list.
 */
class FMessagingDebuggerMessageFilter
{
public:

	/**
	 * Filters the specified message based on the current filter settings.
	 *
	 * @param MessageInfo The message to filter.
	 * @return true if the message passed the filter, false otherwise.
	 */
	bool FilterEndpoint( const FMessageTracerMessageInfoPtr& MessageInfo ) const
	{
		if (!MessageInfo.IsValid())
		{
			return false;
		}

		// @todo gmp: implement message filters

		return true;
	}

public:

	/**
	 * Gets an event delegate that is invoked when the filter settings changed.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT(FMessagingDebuggerMessageFilter, FOnMessagingMessageFilterChanged);
	FOnMessagingMessageFilterChanged& OnChanged()
	{
		return ChangedEvent;
	}

private:

	/** Holds an event delegate that is invoked when the filter settings changed. */
	FOnMessagingMessageFilterChanged ChangedEvent;
};


/** Type definition for shared pointers to instances of FMessagingDebuggerMessageFilter. */
typedef TSharedPtr<FMessagingDebuggerMessageFilter> FMessagingDebuggerMessageFilterPtr;

/** Type definition for shared references to instances of FMessagingDebuggerMessageFilter. */
typedef TSharedRef<FMessagingDebuggerMessageFilter> FMessagingDebuggerMessageFilterRef;
