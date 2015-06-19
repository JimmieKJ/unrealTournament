// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PhATModule.h"
#include "ModuleManager.h"
#include "PhAT.h"
#include "PhATSharedData.h"

const FName PhATAppIdentifier = FName(TEXT("PhATApp"));


/*-----------------------------------------------------------------------------
   FPhATModule
-----------------------------------------------------------------------------*/

class FPhATModule : public IPhATModule
{
public:
	/** Constructor, set up console commands and variables **/
	FPhATModule()
	{
	}

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override
	{
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	}

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();
	}

	virtual TSharedRef<IPhAT> CreatePhAT(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UPhysicsAsset* PhysicsAsset) override
	{
		TSharedRef<FPhAT> NewPhAT(new FPhAT());
		NewPhAT->InitPhAT(Mode, InitToolkitHost, PhysicsAsset);
		return NewPhAT;
	}

	virtual void OpenNewBodyDlg(FPhysAssetCreateParams* NewBodyData, EAppReturnType::Type* NewBodyResponse) override
	{
		FPhATSharedData::OpenNewBodyDlg(NewBodyData, NewBodyResponse);
	}

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE(FPhATModule, PhAT);
