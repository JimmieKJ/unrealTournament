// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "ISubstanceEditor.h"

namespace SubstanceEditorModule
{
	extern const FName SubstanceEditorAppIdentifier;
}


/*-----------------------------------------------------------------------------
   ISubstanceEditorModule
-----------------------------------------------------------------------------*/

class ISubstanceEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new Font editor */
	virtual TSharedRef<ISubstanceEditor> CreateSubstanceEditor(const TSharedPtr< IToolkitHost >& InitToolkitHost, USubstanceGraphInstance* Font) = 0;
};
