// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"



extern const FName SoundClassEditorAppIdentifier;

/*-----------------------------------------------------------------------------
   ISoundCueEditor
-----------------------------------------------------------------------------*/

class ISoundClassEditor : public FAssetEditorToolkit
{
public:
	/**
	 * Creates a new sound class
	 *
	 * @param	FromPin		Pin that was dragged to create sound class
	 * @param	Location	Location for new sound class
	 * @param	Name		Name of the new sound class
	 */
	virtual void CreateSoundClass(class UEdGraphPin* FromPin, const FVector2D& Location, FString Name) = 0;
};

/**
 * Sound class editor module interface
 */
class ISoundClassEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/**
	 * Creates a new sound class editor for a sound class object
	 */
	virtual TSharedRef<FAssetEditorToolkit> CreateSoundClassEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundClass* InSoundClass ) = 0;
};
