// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for input binding editor modules.
 */
class IInputBindingEditorModule
	: public IModuleInterface
{
public:

	/**
	 * Creates an input binding editor panel widget.
	 *
	 * @return The created widget.
	 * @see DestroyInputBindingEditorPanel
	 */
	virtual TWeakPtr<SWidget> CreateInputBindingEditorPanel( ) = 0;

	/**
	 * Destroys a previously created editor panel widget.
	 *
	 * @param Panel The panel to destroy.
	 * CreateInputBindingEditorPanel
	 */
	virtual void DestroyInputBindingEditorPanel( const TWeakPtr<SWidget>& Panel ) = 0;

public:

	/** Virtual destructor. */
	virtual ~IInputBindingEditorModule( ) { }
};
