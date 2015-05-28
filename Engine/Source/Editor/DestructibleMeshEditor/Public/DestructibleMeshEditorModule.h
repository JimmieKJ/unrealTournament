// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "Toolkits/IToolkit.h"	// For EAssetEditorMode
#include "Toolkits/AssetEditorToolkit.h" // For FExtensibilityManager

extern const FName DestructibleMeshEditorAppIdentifier;

class IDestructibleMeshEditor;

/** DestructibleMesh Editor module */
class FDestructibleMeshEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{

public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Creates an instance of table editor object.  Only virtual so that it can be called across the DLL boundary.
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	Table					The table to start editing
	 *
	 * @return	Interface to the new table editor
	 */
	virtual TSharedRef<IDestructibleMeshEditor> CreateDestructibleMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UDestructibleMesh* Table );

	/**
	 * Creates a DestructibleMesh from a StaticMesh
	 *
	 * @param InParent - parent object for the UDestructibleMesh
	 * @param StaticMesh - the StaticMesh to convert
	 * @param Name - the Unreal name for the UDestructibleMesh.  If Name == NAME_None, then a the StaticMesh name followed appended with "_Dest" will be used.
	 * @param Flags - object flags for the UDestructibleMesh
	 * @param OutErrorMsg - if it returns false, this string contains the error
	 *
	 * @return The newly created UDestructibleMesh if successful, NULL otherwise
	 */
	virtual UDestructibleMesh*	CreateDestructibleMeshFromStaticMesh(UObject* InParent, UStaticMesh* StaticMesh, FName Name, EObjectFlags Flags, FText& OutErrorMsg);

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override {return ToolBarExtensibilityManager;}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};


