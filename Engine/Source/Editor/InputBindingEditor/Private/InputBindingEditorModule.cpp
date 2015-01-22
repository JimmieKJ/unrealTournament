// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InputBindingEditorPrivatePCH.h"


/**
 * Implements the LauncherUI module.
 */
class FInputBindingEditorModule
	: public IInputBindingEditorModule
{
public:

	// IInputBindingEditorModule interface

	virtual TWeakPtr<SWidget> CreateInputBindingEditorPanel( ) override
	{
		TSharedPtr<SWidget> Panel = SNew(SInputBindingEditorPanel);
		BindingEditorPanels.Add(Panel);

		return Panel;
	}

	virtual void DestroyInputBindingEditorPanel( const TWeakPtr<SWidget>& Panel ) override
	{
		BindingEditorPanels.Remove(Panel.Pin());
	}

private:

	/** Holds the collection of created binding editor panels. */
	TArray<TSharedPtr<SWidget> > BindingEditorPanels;
};


IMPLEMENT_MODULE(FInputBindingEditorModule, InputBindingEditor);
