// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"


// forward declarations
class FWorkspaceItem;
class SWidget;
class ISlateAtlasProvider;


/**
 * Interface for messaging modules.
 */
class ISlateReflectorModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a widget reflector widget.
	 *
	 * @return The widget.
	 */
	virtual TSharedRef<SWidget> GetWidgetReflector() = 0;

	/**
	 * Create an atlas visualizer widget using the given atlas provider.
	 *
	 * @return The widget.
	 */
	virtual TSharedRef<SWidget> GetAtlasVisualizer( ISlateAtlasProvider* InAtlasProvider ) = 0;

	/**
	 * Create an atlas visualizer widget for the texture atlas provider used by the current renderer.
	 *
	 * @return The widget.
	 */
	virtual TSharedRef<SWidget> GetTextureAtlasVisualizer() = 0;

	/**
	 * Create an atlas visualizer widget for the font atlas provider used by the current renderer.
	 *
	 * @return The widget.
	 */
	virtual TSharedRef<SWidget> GetFontAtlasVisualizer() = 0;

	/**
	 * Display the widget reflector, either spawned from a tab manager, or in a new window if the tab manager can't be used
	 */
	virtual void DisplayWidgetReflector() = 0;

	/**
	 * Display the texture atlas visualizer, either spawned from a tab manager, or in a new window if the tab manager can't be used
	 */
	virtual void DisplayTextureAtlasVisualizer() = 0;

	/**
	 * Display the texture atlas visualizer, either spawned from a tab manager, or in a new window if the tab manager can't be used
	 */
	virtual void DisplayFontAtlasVisualizer() = 0;

	/**
	 * Registers a tab spawner for the widget reflector.
	 *
	 * @param WorkspaceGroup The workspace group to insert the tab into.
	 */
	virtual void RegisterTabSpawner( const TSharedRef<FWorkspaceItem>& WorkspaceGroup ) = 0;

	/** Unregisters the tab spawner for the widget reflector. */
	virtual void UnregisterTabSpawner() = 0;

public:

	/** Virtual destructor. */
	virtual ~ISlateReflectorModule() { }
};
