// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "Sound/SoundClass.h"
#include "Editor/SoundClassEditor/Public/SoundClassEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundClass::GetSupportedClass() const
{
	return USoundClass::StaticClass();
}

void FAssetTypeActions_SoundClass::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		USoundClass* SoundClass = Cast<USoundClass>(*ObjIt);
		if (SoundClass != NULL)
		{
			ISoundClassEditorModule* SoundClassEditorModule = &FModuleManager::LoadModuleChecked<ISoundClassEditorModule>( "SoundClassEditor" );
			SoundClassEditorModule->CreateSoundClassEditor(Mode, EditWithinLevelEditor, SoundClass);
		}
	}
}

#undef LOCTEXT_NAMESPACE