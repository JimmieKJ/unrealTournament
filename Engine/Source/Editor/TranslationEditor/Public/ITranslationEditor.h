// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkit.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "TranslationEditorModule.h"


/** Translation Editor public interface */
class ITranslationEditor : public FAssetEditorToolkit
{

public:

	static void OpenTranslationEditor(const FString& ManifestFile, const FString& ArchiveFile)
	{
		FTranslationEditorModule& TranslationEditorModule = FModuleManager::LoadModuleChecked<FTranslationEditorModule>( "TranslationEditor" );
		TSharedRef< FTranslationEditor > NewTranslationEditor = TranslationEditorModule.CreateTranslationEditor(ManifestFile, ArchiveFile);
	}

};


