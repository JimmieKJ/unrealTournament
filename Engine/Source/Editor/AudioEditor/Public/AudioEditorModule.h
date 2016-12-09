// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "ISoundClassEditor.h"
#include "ISoundCueEditor.h"

class UDialogueWave;
class USoundClass;
class USoundCue;
class USoundNode;
class USoundWave;
struct FDialogueContextMapping;

AUDIOEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogAudioEditor, Log, All);

extern const FName AudioEditorAppIdentifier;

/** Sound class editor module interface */
class IAudioEditorModule :	public IModuleInterface
{
public:
	virtual void RegisterAssetActions() = 0;

	/* Creates a new sound class editor for a sound class object. */
	virtual TSharedRef<FAssetEditorToolkit> CreateSoundClassEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundClass* InSoundClass ) = 0;

	/** Returns the menu extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundClassMenuExtensibilityManager() = 0;

	/** Returns the toolbar extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundClassToolBarExtensibilityManager() = 0;

	/** Creates a new material editor, either for a material or a material function. */
	virtual TSharedRef<ISoundCueEditor> CreateSoundCueEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundCue* SoundCue) = 0;

	/** Returns the menu extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundCueMenuExtensibilityManager() = 0;

	/** Returns the toolbar extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundCueToolBarExtensibilityManager() = 0;

	/** Replaces sound cue nodes in the graph. */
	virtual void ReplaceSoundNodesInGraph(USoundCue* SoundCue, UDialogueWave* DialogueWave, TArray<USoundNode*>& NodesToReplace, const FDialogueContextMapping& ContextMapping) = 0;

	/** Imports a sound wave from the given package. */
	virtual USoundWave* ImportSoundWave(UPackage* const SoundWavePackage, const FString& InSoundWaveAssetName, const FString& InWavFilename) = 0;
};
