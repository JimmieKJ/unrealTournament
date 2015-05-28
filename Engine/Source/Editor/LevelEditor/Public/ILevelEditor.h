// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"


/**
 * Public interface to SLevelEditor
 */
class ILevelEditor : public SCompoundWidget, public IToolkitHost
{

public:

	/** Summons a context menu for this level editor at the mouse cursor's location */
	virtual void SummonLevelViewportContextMenu() = 0;

	/** Returns a list of all of the toolkits that are currently hosted by this toolkit host */
	virtual const TArray< TSharedPtr< IToolkit > >& GetHostedToolkits() const = 0;

	/** Gets an array of all viewports in this level editor */
	virtual TArray< TSharedPtr< ILevelViewport > > GetViewports() const = 0;
	
	/** Gets the active level viewport for this level editor */
	virtual TSharedPtr<ILevelViewport> GetActiveViewportInterface() = 0;

	/** Get the thumbnail pool used by this level editor */
	virtual TSharedPtr< class FAssetThumbnailPool > GetThumbnailPool() const = 0;

	/** Append commands to the command list for the level editor */
	virtual void AppendCommands( const TSharedRef<FUICommandList>& InCommandsToAppend ) = 0;
};


