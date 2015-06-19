// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkit.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "TranslationEditorModule.h"
#include "../Private/TranslationPickerWidget.h"


/** Translation Editor public interface */
class ITranslationEditor : public FAssetEditorToolkit
{

public:

	TRANSLATIONEDITOR_API static void OpenTranslationEditor(const FString& InManifestFile, const FString& InArchiveFile);

	TRANSLATIONEDITOR_API static void OpenTranslationPicker();

	ITranslationEditor(const FString& InManifestFile, const FString& InArchiveFile)
		: ManifestFilePath(InManifestFile)
		, ArchiveFilePath(InArchiveFile)
	{}

	virtual bool OnRequestClose() override;

protected:

	/** Name of the project we are translating for */
	FString ManifestFilePath;
	/** Name of the language we are translating to */
	FString ArchiveFilePath;

private:

	/** To keep track of what translation editors are open editing which manifest files */
	static TMap<FString, ITranslationEditor*> OpenTranslationEditors;
	
	/** Called on open to add this to our list of open translation editors */
	void RegisterTranslationEditor();
	
	/** Called on close to remove this from our list of open translation editors */
	void UnregisterTranslationEditor();
};


