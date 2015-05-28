// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "TranslationEditorPrivatePCH.h"
#include "ITranslationEditor.h"
#include "Editor/TranslationEditor/Private/TranslationPickerWidget.h"

/** To keep track of what translation editors are open editing which manifest files */
TMap<FString, ITranslationEditor*> ITranslationEditor::OpenTranslationEditors = TMap<FString, ITranslationEditor*>();

void ITranslationEditor::OpenTranslationEditor(const FString& InManifestFile, const FString& InArchiveFile)
{
	// Only open a new translation editor if there isn't another one open for this archive file already
	if (!OpenTranslationEditors.Contains(InArchiveFile))
	{
		FTranslationEditorModule& TranslationEditorModule = FModuleManager::LoadModuleChecked<FTranslationEditorModule>("TranslationEditor");
		ITranslationEditor* NewTranslationEditor = (ITranslationEditor*)&(TranslationEditorModule.CreateTranslationEditor(InManifestFile, InArchiveFile).Get());
		NewTranslationEditor->RegisterTranslationEditor();
	}
	else
	{
		// If already editing this archive file, flash the tab that contains the editor that has that file open
		ITranslationEditor* OpenEditor = *OpenTranslationEditors.Find(InArchiveFile);
		OpenEditor->FocusWindow();
	}
}

void ITranslationEditor::OpenTranslationPicker()
{
	if (!TranslationPickerManager::IsPickerWindowOpen())
	{
		TranslationPickerManager::OpenPickerWindow();
	}
}

void ITranslationEditor::RegisterTranslationEditor()
{
	ITranslationEditor::OpenTranslationEditors.Add(ArchiveFilePath, this);
}

void ITranslationEditor::UnregisterTranslationEditor()
{
	OpenTranslationEditors.Remove(ArchiveFilePath);
}

bool ITranslationEditor::OnRequestClose()
{
	ITranslationEditor::UnregisterTranslationEditor();

	return true;
}




