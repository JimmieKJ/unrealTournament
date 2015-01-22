// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEd.h"
#include "ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"
#include "ISoundCueEditor.h"

extern const FName SoundCueEditorAppIdentifier;

DECLARE_LOG_CATEGORY_EXTERN(LogSoundCueEditor, Log, All);


/*-----------------------------------------------------------------------------
   ISoundCueEditorModule
-----------------------------------------------------------------------------*/

class ISoundCueEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new material editor, either for a material or a material function */
	virtual TSharedRef<ISoundCueEditor> CreateSoundCueEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundCue* SoundCue) = 0;
};
