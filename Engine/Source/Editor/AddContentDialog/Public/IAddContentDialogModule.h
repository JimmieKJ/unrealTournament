// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

class FContentSourceProviderManager;

/** Defines methods for interacting with the Add Content Dialog.  */
class IAddContentDialogModule : public IModuleInterface
{

public:
	/** Gets the object responsible for managing content source providers. */
	virtual TSharedRef<FContentSourceProviderManager> GetContentSourceProviderManager() = 0;

	/** Creates a dialog for adding existing content to a project. */
	virtual void ShowDialog(TSharedRef<SWindow> ParentWindow) = 0;
};