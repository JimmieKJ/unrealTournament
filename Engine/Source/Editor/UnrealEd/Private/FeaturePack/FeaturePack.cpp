// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "FeaturePack.h"
#include "AssetToolsModule.h"
#include "AssetEditorManager.h"
#include "AssetRegistryModule.h"
#include "LevelEditor.h"
#include "Toolkits/AssetEditorManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFeaturePack, Log, All);

FFeaturePack::FFeaturePack()
{
}

void FFeaturePack::ImportPendingPacks()
{	
	bool bAddPacks;
	if (GConfig->GetBool(TEXT("StartupActions"), TEXT("bAddPacks"), bAddPacks, GGameIni) == true)
	{
		if (bAddPacks == true)
		{
			ParsePacks();
			UE_LOG(LogFeaturePack, Warning, TEXT("Inserted %d feature packs"), ImportedPacks.Num() );
 			GConfig->SetBool(TEXT("StartupActions"), TEXT("bAddPacks"), false, GGameIni);
 			GConfig->Flush(true, GGameIni);
		}
	}	
}

void FFeaturePack::ParsePacks()
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	// Look for pack insertions in the startup actions section
	TArray<FString> PacksToAdd;
	GConfig->GetArray(TEXT("StartupActions"), TEXT("InsertPack"), PacksToAdd, GGameIni);
	for (int32 iPackEntry = 0; iPackEntry < PacksToAdd.Num(); iPackEntry++)
	{
		PackData EachPackData;
		TArray<FString> PackEntries;
		PacksToAdd[iPackEntry].ParseIntoArray(PackEntries, TEXT(","), true);
		FString PackSource;
		FString PackName;
		// Parse the pack name and source
		for (int32 iEntry = 0; iEntry < PackEntries.Num(); iEntry++)
		{
			FString EachString = PackEntries[iEntry];
			// String the parenthesis
			EachString = EachString.Replace(TEXT("("), TEXT(""));
			EachString = EachString.Replace(TEXT(")"), TEXT(""));
			if (EachString.StartsWith(TEXT("PackSource=")) == true)
			{
				EachString = EachString.Replace(TEXT("PackSource="), TEXT(""));
				EachString = EachString.TrimQuotes();
				EachPackData.PackSource = EachString;
			}
			if (EachString.StartsWith(TEXT("PackName=")) == true)
			{
				EachString = EachString.Replace(TEXT("PackName="), TEXT(""));
				EachString = EachString.TrimQuotes();
				EachPackData.PackName = EachString;
			}
		}
		
		// If we found anything to insert, insert it !
		if ((EachPackData.PackSource.IsEmpty() == false) && (EachPackData.PackName.IsEmpty() == false))
		{
			TArray<FString> EachImport;
			FString FullPath = FPaths::FeaturePackDir() + EachPackData.PackSource;
			EachImport.Add(FullPath);
			EachPackData.ImportedObjects = AssetToolsModule.Get().ImportAssets(EachImport, TEXT("/Game"), nullptr, false);
						
			if (EachPackData.ImportedObjects.Num() == 0)
			{			
				UE_LOG(LogFeaturePack, Warning, TEXT("No objects imported installing pack %s"), *EachPackData.PackSource);
			}
			else
			{
				// Save any imported assets.
				TArray<UPackage*> ToSave;
				for (auto ImportedObject : EachPackData.ImportedObjects)
				{
					ToSave.AddUnique(ImportedObject->GetOutermost());
				}
				FEditorFileUtils::PromptForCheckoutAndSave(ToSave, /*bCheckDirty=*/ false, /*bPromptToSave=*/ false);				
			}

			ImportedPacks.Add(EachPackData);
		}
	}
}


#undef  LOCTEXT_NAMESPACE
