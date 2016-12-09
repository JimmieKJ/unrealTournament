// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#ifndef __StaticMeshEditorModule_h__
#define __StaticMeshEditorModule_h__

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"

class IStaticMeshEditor;
class UStaticMesh;

extern const FName StaticMeshEditorAppIdentifier;

/**
 * Static mesh editor module interface
 */
class IStaticMeshEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/**
	 * Creates a new static mesh editor.
	 */
	virtual TSharedRef<IStaticMeshEditor> CreateStaticMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UStaticMesh* StaticMesh ) = 0;
};

#endif // __StaticMeshEditorModule_h__
