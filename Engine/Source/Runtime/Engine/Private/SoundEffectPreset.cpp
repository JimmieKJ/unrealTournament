// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundEffectPreset.h"

#if WITH_EDITOR
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorManager.h"
#include "Toolkits/SimpleAssetEditor.h"
#endif


/*-----------------------------------------------------------------------------
	FAssetTypeActions_SoundEffectPreset Implementation
-----------------------------------------------------------------------------*/

#if WITH_EDITOR
FAssetTypeActions_SoundEffectPreset::FAssetTypeActions_SoundEffectPreset(USoundEffectPreset* InEffectPreset)
	: EffectPreset(InEffectPreset)
{
}

bool FAssetTypeActions_SoundEffectPreset::HasActions(const TArray<UObject*>& InObjects) const
{
	return false;
}

void FAssetTypeActions_SoundEffectPreset::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
}

void FAssetTypeActions_SoundEffectPreset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

void FAssetTypeActions_SoundEffectPreset::AssetsActivated(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
{
	if (ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened)
	{
		if (InObjects.Num() == 1)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(InObjects[0]);
		}
		else if (InObjects.Num() > 1)
		{
			FAssetEditorManager::Get().OpenEditorForAssets(InObjects);
		}
	}
}

bool FAssetTypeActions_SoundEffectPreset::CanFilter()
{
	return true;
}

bool FAssetTypeActions_SoundEffectPreset::CanLocalize() const
{
	return false;
}

bool FAssetTypeActions_SoundEffectPreset::CanMerge() const
{
	return false;
}

void FAssetTypeActions_SoundEffectPreset::Merge(UObject* InObject)
{
}

void FAssetTypeActions_SoundEffectPreset::Merge(UObject* BaseAsset, UObject* RemoteAsset, UObject* LocalAsset, const FOnMergeResolved& ResolutionCallback)
{
}

bool FAssetTypeActions_SoundEffectPreset::ShouldForceWorldCentric()
{
	return false;
}

void FAssetTypeActions_SoundEffectPreset::PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const
{
	check(OldAsset != nullptr);
	check(NewAsset != nullptr);

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	// Dump assets to temp text files
	FString OldTextFilename = AssetToolsModule.Get().DumpAssetToTempFile(OldAsset);
	FString NewTextFilename = AssetToolsModule.Get().DumpAssetToTempFile(NewAsset);
	FString DiffCommand = GetDefault<UEditorLoadingSavingSettings>()->TextDiffToolPath.FilePath;

	AssetToolsModule.Get().CreateDiffProcess(DiffCommand, OldTextFilename, NewTextFilename);
}

class UThumbnailInfo* FAssetTypeActions_SoundEffectPreset::GetThumbnailInfo(UObject* Asset) const
{
	return nullptr;
}

TSharedPtr<class SWidget> FAssetTypeActions_SoundEffectPreset::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	return nullptr;
}

FText FAssetTypeActions_SoundEffectPreset::GetAssetDescription(const class FAssetData& AssetData) const
{
	return FText::GetEmpty();
}

bool FAssetTypeActions_SoundEffectPreset::IsImportedAsset() const
{
	return false;
}

void FAssetTypeActions_SoundEffectPreset::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
}

FText FAssetTypeActions_SoundEffectPreset::GetName() const
{ 
	return EffectPreset->GetAssetActionName(); 
}

UClass* FAssetTypeActions_SoundEffectPreset::GetSupportedClass() const
{ 
	return EffectPreset->GetClass();
}

#endif

/*-----------------------------------------------------------------------------
	USoundEffectPresetBase Implementation
-----------------------------------------------------------------------------*/

USoundEffectPreset::USoundEffectPreset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PresetSettings.Data = GetSettings();
	PresetSettings.Size = GetSettingsSize();
	PresetSettings.Struct = GetSettingsStruct();
}

