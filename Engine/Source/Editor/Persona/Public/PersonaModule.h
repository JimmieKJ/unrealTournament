// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __PersonaModule_h__
#define __PersonaModule_h__

#pragma once

#include "Toolkits/IToolkit.h"		// For EToolkitMode::Type
#include "Toolkits/AssetEditorToolkit.h" 

extern const FName PersonaAppName;

/**
 * Persona module manages the lifetime of all instances of Persona editors.
 */
class FPersonaModule : public IModuleInterface,
	public IHasMenuExtensibility
{
public:
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule();

	/**
	 * Creates an instance of a Persona editor.  Only virtual so that it can be called across the DLL boundary.
	 *
	 * Note: This function should not be called directly, use one of the following instead:
	 *	- FKismetEditorUtilities::BringKismetToFocusAttentionOnObject
	 *  - FAssetEditorManager::Get().OpenEditorForAsset
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	AnimBlueprint			The blueprint object to start editing.  If specified, Skeleton and AnimationAsset must be NULL.
	 * @param	Skeleton				The skeleton to edit.  If specified, Blueprint must be NULL.
	 * @param	AnimationAsset			The animation asset to edit.  If specified, Blueprint must be NULL.
	 *
	 * @return	Interface to the new Persona editor
	 */
	virtual TSharedRef<class IBlueprintEditor> CreatePersona( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, class USkeleton* Skeleton, class UAnimBlueprint* Blueprint, class UAnimationAsset* AnimationAsset, class USkeletalMesh * Mesh );

	/** Gets the extensibility managers for outside entities to extend persona editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() {return ToolBarExtensibilityManager;}

private:
	/** When a new AnimBlueprint is created, this will handle post creation work such as adding non-event default nodes */
	void OnNewBlueprintCreated(UBlueprint* InBlueprint);

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};



#endif		//__PersonaModule_h__
