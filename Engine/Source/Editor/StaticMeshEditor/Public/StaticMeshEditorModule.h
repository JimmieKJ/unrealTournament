// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __StaticMeshEditorModule_h__
#define __StaticMeshEditorModule_h__

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "IStaticMeshEditor.h"

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
