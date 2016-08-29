// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AutomationWindowPrivatePCH.h"


FAutomationTestPresetManager::FAutomationTestPresetManager()
{
	// Add the None Option
	Presets.Add(AutomationPresetPtr());
}


AutomationPresetRef FAutomationTestPresetManager::AddNewPreset( )
{
	AutomationPresetPtr NewPreset = MakeShareable(new FAutomationTestPreset());
	Presets.Add(NewPreset);
	return NewPreset.ToSharedRef();
}


AutomationPresetRef FAutomationTestPresetManager::AddNewPreset( const FText& PresetName, const TArray<FString>& SelectedTests )
{
	AutomationPresetRef NewPreset = AddNewPreset();
	NewPreset->SetPresetName(PresetName);
	NewPreset->SetEnabledTests(SelectedTests);
	SavePreset(NewPreset);
	return NewPreset;
}


TArray<AutomationPresetPtr>& FAutomationTestPresetManager::GetAllPresets( )
{
	return Presets;
}


AutomationPresetPtr FAutomationTestPresetManager::LoadPreset( FArchive& Archive )
{
	FAutomationTestPreset* NewPreset = new FAutomationTestPreset();

	if(NewPreset->Serialize(Archive))
	{
		return MakeShareable(NewPreset);
	}

	delete NewPreset;

	return nullptr;
}


void FAutomationTestPresetManager::RemovePreset( const AutomationPresetRef Preset )
{
	if (Presets.Remove(Preset) > 0)
	{
		// Find the name of the preset on disk
		FString PresetFileName = GetPresetFolder() / Preset->GetID().ToString() + TEXT(".uap");

		// delete the preset on disk
		IFileManager::Get().Delete(*PresetFileName);
	}
}


void FAutomationTestPresetManager::SavePreset( const AutomationPresetRef Preset )
{
	FString PresetFileName = GetPresetFolder() / Preset->GetID().ToString() + TEXT(".uap");
	FArchive* PresetFileWriter = IFileManager::Get().CreateFileWriter(*PresetFileName);

	if (PresetFileWriter != nullptr)
	{
		SavePreset(Preset, *PresetFileWriter);

		delete PresetFileWriter;
	}
}


void FAutomationTestPresetManager::SavePreset( const AutomationPresetRef Preset, FArchive& Archive )
{
	Preset->Serialize(Archive);
}


void FAutomationTestPresetManager::LoadPresets( )
{
	TArray<FString> PresetFileNames;

	IFileManager::Get().FindFiles(PresetFileNames, *(GetPresetFolder() / TEXT("*.uap")), true, false);

	for (TArray<FString>::TConstIterator It(PresetFileNames); It; ++It)
	{
		FString PresetFilePath = GetPresetFolder() / *It;
		FArchive* PresetFileReader = IFileManager::Get().CreateFileReader(*PresetFilePath);

		if (PresetFileReader != nullptr)
		{
			AutomationPresetPtr LoadedPreset = LoadPreset(*PresetFileReader);

			if (LoadedPreset.IsValid())
			{
				Presets.Add(LoadedPreset);
			}
			else
			{
				IFileManager::Get().Delete(*PresetFilePath);
			}

			delete PresetFileReader;
		}
	}
}
