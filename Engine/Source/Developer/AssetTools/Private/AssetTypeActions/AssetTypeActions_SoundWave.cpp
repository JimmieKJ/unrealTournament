// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "AssetToolsModule.h"
#include "SoundDefinitions.h"
#include "ContentBrowserModule.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundWave::GetSupportedClass() const
{
	return USoundWave::StaticClass();
}

void FAssetTypeActions_SoundWave::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	FAssetTypeActions_SoundBase::GetActions(InObjects, MenuBuilder);

	auto SoundNodes = GetTypedWeakObjectPtrs<USoundWave>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SoundWave_CreateCue", "Create Cue"),
		LOCTEXT("SoundWave_CreateCueTooltip", "Creates a sound cue using this sound wave."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.SoundCue"),
		FUIAction(
		FExecuteAction::CreateSP( this, &FAssetTypeActions_SoundWave::ExecuteCreateSoundCue, SoundNodes ),
		FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_SoundWave::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto SoundWave = CastChecked<USoundWave>(Asset);
		OutSourceFilePaths.Add(FReimportManager::ResolveImportFilename(SoundWave->SourceFilePath, SoundWave));
	}
}

void FAssetTypeActions_SoundWave::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

void FAssetTypeActions_SoundWave::ExecuteCreateSoundCue(TArray<TWeakObjectPtr<USoundWave>> Objects)
{
	const FString DefaultSuffix = TEXT("_Cue");

	if ( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
			Factory->InitialSoundWave = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USoundCue::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object )
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
				Factory->InitialSoundWave = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USoundCue::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

#undef LOCTEXT_NAMESPACE
