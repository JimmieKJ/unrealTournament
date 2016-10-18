// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

/**
 * IConnectionBasedMessagingModule interface allows adding/removing of connections
 */
class IConnectionBasedMessagingModule
	: public IModuleInterface
{
public:

	// IConnectionBasedMessagingModule interface

	virtual void AddOutgoingConnection(const FString& Endpoint) = 0;
	virtual void RemoveOutgoingConnection(const FString& Endpoint) = 0;
};
