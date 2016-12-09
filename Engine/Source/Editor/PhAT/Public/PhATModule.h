// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

class UPhysicsAsset;

DECLARE_LOG_CATEGORY_EXTERN(LogPhAT, Log, All);


/*-----------------------------------------------------------------------------
   IPhATModule
-----------------------------------------------------------------------------*/

class IPhATModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new PhAT instance */
	virtual TSharedRef<class IPhAT> CreatePhAT(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UPhysicsAsset* PhysicsAsset) = 0;

	/** Opens a "New Asset/Body" modal dialog window */
	virtual void OpenNewBodyDlg(struct FPhysAssetCreateParams* NewBodyData, EAppReturnType::Type* NewBodyResponse) = 0;
};

