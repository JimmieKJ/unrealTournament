// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __MatineeModule_h__
#define __MatineeModule_h__

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "IMatinee.h"

extern const FName MatineeAppIdentifier;

//class FMatinee;

/*-----------------------------------------------------------------------------
   IMatineeModule
-----------------------------------------------------------------------------*/

class IMatineeModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new Matinee instance */
	virtual TSharedRef<IMatinee> CreateMatinee(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, AMatineeActor* MatineeActor) = 0;

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IMatineeModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IMatineeModule >("Matinee");
	}

	/**
	* Delegate for binding functions to be called when the Matinee editor is created.
	*/
	DECLARE_EVENT(IMainFrameModule, FMatineeEditorOpenedEvent);
	virtual FMatineeEditorOpenedEvent& OnMatineeEditorOpened() = 0;
};


#endif // __MatineeModule_h__
