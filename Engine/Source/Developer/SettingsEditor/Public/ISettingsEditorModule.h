// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"


// forward declarations
class ISettingsContainer;
class ISettingsEditorModel;
class SWidget;


/**
 * Interface for settings editor modules.
 */
class ISettingsEditorModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a settings editor widget.
	 *
	 * @param Model The view model.
	 * @return The new widget.
	 * @see CreateCodel
	 */
	virtual TSharedRef<SWidget> CreateEditor( const TSharedRef<ISettingsEditorModel>& Model ) = 0;

	/**
	 * Creates a view model for the settings editor widget.
	 *
	 * @param SettingsContainer The settings container.
	 * @return The controller.
	 * @see CreateEditor
	 */
	virtual ISettingsEditorModelRef CreateModel( const TSharedRef<ISettingsContainer>& SettingsContainer ) = 0;

public:

	/** Virtual destructor. */
	virtual ~ISettingsEditorModule() { }
};
