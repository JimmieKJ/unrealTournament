// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a view model for the messaging debugger.
 */
class FMessagingDebuggerModel
{
public:

	/**
	 * Clears all visibility filters (to show all messages).
	 */
	void ClearVisibilities()
	{
		InvisibleEndpoints.Empty();
		InvisibleTypes.Empty();

		MessageVisibilityChangedEvent.Broadcast();
	}

	/**
	 * Gets the endpoint that is currently selected in the endpoint list.
	 *
	 * @return The selected endpoint.
	 */
	FMessageTracerEndpointInfoPtr GetSelectedEndpoint() const
	{
		return SelectedEndpoint;
	}

	/**
	 * Gets the message that is currently selected in the message history.
	 *
	 * @return The selected message.
	 */
	FMessageTracerMessageInfoPtr GetSelectedMessage() const
	{
		return SelectedMessage;
	}

	/** 
	 * Checks whether messages of the given message endpoint should be visible.
	 *
	 * @param Endpoint The information of the endpoint to check.
	 * @return true if the endpoint's messages should be visible, false otherwise.
	 */
	bool IsEndpointVisible( const FMessageTracerEndpointInfoRef& EndpointInfo ) const
	{
		return !InvisibleEndpoints.Contains(EndpointInfo);
	}

	/**
	 * Checks whether the given message should be visible.
	 *
	 * @param MessageInfo The information of the message to check.
	 * @return true if the message should be visible, false otherwise.
	 */
	bool IsMessageVisible( const FMessageTracerMessageInfoRef& MessageInfo ) const
	{
		if (!IsEndpointVisible(MessageInfo->SenderInfo.ToSharedRef()) ||
			!IsTypeVisible(MessageInfo->TypeInfo.ToSharedRef()))
		{
			return false;
		}

		if (MessageInfo->DispatchStates.Num() > 0)
		{
			for (TMap<FMessageTracerEndpointInfoPtr, FMessageTracerDispatchStatePtr>::TConstIterator It(MessageInfo->DispatchStates); It; ++It)
			{
				const FMessageTracerDispatchStatePtr& DispatchState = It.Value();

				if (IsEndpointVisible(DispatchState->EndpointInfo.ToSharedRef()))
				{
					return true;
				}
			}
		}

		return true;
	}

	/**
	 * Checks whether messages of the given type should be visible.
	 *
	 * @param TypeInfo The information of the message type to check.
	 * @return true if messages of the given type should be visible, false otherwise.
	 */
	bool IsTypeVisible( const FMessageTracerTypeInfoRef& TypeInfo ) const
	{
		return !InvisibleTypes.Contains(TypeInfo);
	}

	/**
	 * Selects the specified endpoint (or none if nullptr).
	 *
	 * @param EndpointInfo The information of the endpoint to select.
	 */
	void SelectEndpoint( const FMessageTracerEndpointInfoPtr& EndpointInfo )
	{
		if (SelectedEndpoint != EndpointInfo)
		{
			SelectedEndpoint = EndpointInfo;
			SelectedEndpointChangedEvent.Broadcast();
		}
	}

	/**
	 * Selects the specified message (or none if nullptr).
	 *
	 * @param MessageInfo The information of the message to select.
	 */
	void SelectMessage( const FMessageTracerMessageInfoPtr& MessageInfo )
	{
		if (SelectedMessage != MessageInfo)
		{
			SelectedMessage = MessageInfo;
			SelectedMessageChangedEvent.Broadcast();
		}
	}

	/**
	 * Sets whether messages for the given endpoint should be visible.
	 *
	 * @param Endpoint The information for the endpoint.
	 * @param Visible Whether the endpoint's messages should be visible or not.
	 */
	void SetEndpointVisibility( const FMessageTracerEndpointInfoRef& EndpointInfo, bool Visible )
	{
		if (Visible)
		{
			InvisibleEndpoints.Remove(EndpointInfo);
		}
		else
		{
			InvisibleEndpoints.AddUnique(EndpointInfo);
		}

		MessageVisibilityChangedEvent.Broadcast();
	}

	/**
	 * Sets whether messages for the given message type should be visible.
	 *
	 * @param Endpoint The information for the message type.
	 * @param Visible Whether messages of the given type should be visible or not.
	 */
	void SetTypeVisibility( const FMessageTracerTypeInfoRef& TypeInfo, bool Visible )
	{
		if (Visible)
		{
			InvisibleTypes.Remove(TypeInfo);
		}
		else
		{
			InvisibleTypes.AddUnique(TypeInfo);
		}

		MessageVisibilityChangedEvent.Broadcast();
	}

public:

	/**
	 * Gets an event delegate that is invoked when the filter settings changed.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT(FMessagingDebuggerModel, FOnMessageVisibilityChanged);
	FOnMessageVisibilityChanged& OnMessageVisibilityChanged()
	{
		return MessageVisibilityChangedEvent;
	}

	/**
	 * Gets an event delegate that is invoked when the selected endpoint has changed.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT(FMessagingDebuggerModel, FOnSelectedEndpointChanged);
	FOnSelectedEndpointChanged& OnSelectedEndpointChanged()
	{
		return SelectedEndpointChangedEvent;
	}

	/**
	 * Gets an event delegate that is invoked when the selected message has changed.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT(FMessagingDebuggerModel, FOnSelectedMessageChanged);
	FOnSelectedMessageChanged& OnSelectedMessageChanged()
	{
		return SelectedMessageChangedEvent;
	}

private:

	/** Holds the collection of invisible message endpoints. */
	TArray<FMessageTracerEndpointInfoPtr> InvisibleEndpoints;

	/** Holds the collection of invisible message types. */
	TArray<FMessageTracerTypeInfoPtr> InvisibleTypes;

	/** Holds a pointer to the sole endpoint that is currently selected in the endpoint list. */
	FMessageTracerEndpointInfoPtr SelectedEndpoint;

	/** Holds a pointer to the message that is currently selected in the message history. */
	FMessageTracerMessageInfoPtr SelectedMessage;

private:

	/** Holds an event delegate that is invoked when the visibility of messages has changed. */
	FOnMessageVisibilityChanged MessageVisibilityChangedEvent;

	/** Holds an event delegate that is invoked when the selected endpoint has changed. */
	FOnSelectedEndpointChanged SelectedEndpointChangedEvent;

	/** Holds an event delegate that is invoked when the selected message has changed. */
	FOnSelectedMessageChanged SelectedMessageChangedEvent;
};


/** Type definition for shared pointers to instances of FMessagingDebuggerModel. */
typedef TSharedPtr<FMessagingDebuggerModel> FMessagingDebuggerModelPtr;

/** Type definition for shared references to instances of FMessagingDebuggerTypeFilter. */
typedef TSharedRef<FMessagingDebuggerModel> FMessagingDebuggerModelRef;
