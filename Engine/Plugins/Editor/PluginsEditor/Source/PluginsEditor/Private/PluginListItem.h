// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPluginManager.h"		// For FPluginStatus


typedef TSharedPtr< class FPluginListItem > FPluginListItemPtr;


/**
 * Represents a single plugin in the plugin list
 */
class FPluginListItem
{

public:

	/** Plugin status object, which contains information about the plugin such as its name and description */
	FPluginStatus PluginStatus;


public:

	/** Default constructor for FPluginListItem */
	FPluginListItem()
	{
	}

private:

};


