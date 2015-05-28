// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a filter for the message endpoints list.
 */
class FMessagingDebuggerTypeFilter
{
public:

	/**
	 * Filters the specified message type based on the current filter settings.
	 *
	 * @param TypeInfo The message type to filter.
	 * @return true if the endpoint passed the filter, false otherwise.
	 */
	bool FilterType( const FMessageTracerTypeInfoPtr& TypeInfo ) const
	{
		if (!TypeInfo.IsValid())
		{
			return false;
		}

		// filter by search string
		TArray<FString> FilterSubstrings;
		
		if (FilterString.ParseIntoArray(FilterSubstrings, TEXT(" "), true) > 0)
		{
			FString TypeName = TypeInfo->TypeName.ToString();

			for (int32 SubstringIndex = 0; SubstringIndex < FilterSubstrings.Num(); ++SubstringIndex)
			{
				if (!TypeName.Contains(FilterSubstrings[SubstringIndex]))
				{
					return false;
				}
			}
		}

		return true;
	}

	/**
	 * Sets the filter string.
	 *
	 * @param InFilterString The filter string to set.
	 */
	void SetFilterString( const FString& InFilterString )
	{
		FilterString = InFilterString;
		ChangedEvent.Broadcast();
	}

public:

	/**
	 * Gets an event delegate that is invoked when the filter settings changed.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT(FMessagingDebuggerTypeFilter, FOnMessagingEndpointFilterChanged);
	FOnMessagingEndpointFilterChanged& OnChanged()
	{
		return ChangedEvent;
	}

private:

	/** Holds the filter string used to filter message types by their names. */
	FString FilterString;

private:

	/** Holds an event delegate that is invoked when the filter settings changed. */
	FOnMessagingEndpointFilterChanged ChangedEvent;
};


/** Type definition for shared pointers to instances of FMessagingDebuggerTypeFilter. */
typedef TSharedPtr<class FMessagingDebuggerTypeFilter> FMessagingDebuggerTypeFilterPtr;

/** Type definition for shared references to instances of FMessagingDebuggerTypeFilter. */
typedef TSharedRef<class FMessagingDebuggerTypeFilter> FMessagingDebuggerTypeFilterRef;
