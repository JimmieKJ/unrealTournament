// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __CascadeModule_h__
#define __CascadeModule_h__

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "ICascade.h"
extern const FName CascadeAppIdentifier;

class FCascade;


/*-----------------------------------------------------------------------------
   ICascadeModule
-----------------------------------------------------------------------------*/

class ICascadeModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new Cascade instance */
	virtual TSharedRef<ICascade> CreateCascade(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UParticleSystem* ParticleSystem) = 0;

	/** Removes the specified instance from the list of open Cascade toolkits */
	virtual void CascadeClosed(FCascade* CascadeInstance) = 0;

	/** Refreshes the toolkit inspecting the specified particle system */
	virtual void RefreshCascade(UParticleSystem* ParticleSystem) = 0;

	/** Converts all the modules in the specified particle system to seeded modules */
	virtual void ConvertModulesToSeeded(UParticleSystem* ParticleSystem) = 0;
};

#endif // __CascadeModule_h__
