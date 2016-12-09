// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMessageContext.h"

/**
 * Interface for message handlers.
 */
class IMessageHandler
{
public:

	/**
	 * Handles the specified message.
	 *
	 * @param Context The context of the message to handle.
	 */
	virtual void HandleMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMessageHandler() { }
};
