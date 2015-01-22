// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __PhATModule_h__
#define __PhATModule_h__

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "IPhAT.h"

extern const FName PhATAppIdentifier;

class FPhAT;

DECLARE_LOG_CATEGORY_EXTERN(LogPhAT, Log, All);


/*-----------------------------------------------------------------------------
   IPhATModule
-----------------------------------------------------------------------------*/

class IPhATModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new PhAT instance */
	virtual TSharedRef<IPhAT> CreatePhAT(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UPhysicsAsset* PhysicsAsset) = 0;

	/** Opens a "New Asset/Body" modal dialog window */
	virtual void OpenNewBodyDlg(struct FPhysAssetCreateParams* NewBodyData, EAppReturnType::Type* NewBodyResponse) = 0;
};

#endif // __PhATModule_h__
