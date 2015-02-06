// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AddContentDialogPCH.h"
#include "FeaturePackContentSource.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IPlatformFilePak.h"
#include "FileHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFeaturePack, Log, All);


FFeaturePackContentSource::FFeaturePackContentSource(FString InFeaturePackPath)
{
	FeaturePackPath = InFeaturePackPath;
	bPackValid = false;
	// Create a pak platform file and mount the feature pack file.
	FPakPlatformFile PakPlatformFile;
	FString CommandLine;
	PakPlatformFile.Initialize(&FPlatformFileManager::Get().GetPlatformFile(), TEXT(""));
	FString MountPoint = "root:/";
	PakPlatformFile.Mount(*InFeaturePackPath, 0, *MountPoint);
	
	// Gets the manifest file as a JSon string
	TArray<uint8> ManifestBuffer;
	if( LoadPakFileToBuffer(PakPlatformFile, FPaths::Combine(*MountPoint, TEXT("manifest.json")), ManifestBuffer) == false )
	{
		UE_LOG(LogFeaturePack, Warning, TEXT("Error in Feature pack %s. Cannot find manifest."), *InFeaturePackPath);
		Category = EContentSourceCategory::Unknown;
		return;
	}
	FString ManifestString;
	FFileHelper::BufferToString(ManifestString, ManifestBuffer.GetData(), ManifestBuffer.Num());

	// Populate text fields from the manifest.
	TSharedPtr<FJsonObject> ManifestObject;
	TSharedRef<TJsonReader<>> ManifestReader = TJsonReaderFactory<>::Create(ManifestString);
	FJsonSerializer::Deserialize(ManifestReader, ManifestObject);
	for (TSharedPtr<FJsonValue> NameValue : ManifestObject->GetArrayField("Name"))
	{
		TSharedPtr<FJsonObject> LocalizedNameObject = NameValue->AsObject();
		LocalizedNames.Add(FLocalizedText(
			LocalizedNameObject->GetStringField("Language"),
			FText::FromString(LocalizedNameObject->GetStringField("Text"))));
	}

	for (TSharedPtr<FJsonValue> DescriptionValue : ManifestObject->GetArrayField("Description"))
	{
		TSharedPtr<FJsonObject> LocalizedDescriptionObject = DescriptionValue->AsObject();
		LocalizedDescriptions.Add(FLocalizedText(
			LocalizedDescriptionObject->GetStringField("Language"),
			FText::FromString(LocalizedDescriptionObject->GetStringField("Text"))));
	}

	// Parse asset types field
	for (TSharedPtr<FJsonValue> AssetTypesValue : ManifestObject->GetArrayField("AssetTypes"))
	{
		TSharedPtr<FJsonObject> LocalizedAssetTypesObject = AssetTypesValue->AsObject();
		LocalizedAssetTypesList.Add(FLocalizedText(
			LocalizedAssetTypesObject->GetStringField("Language"),
			FText::FromString(LocalizedAssetTypesObject->GetStringField("Text"))));
	}
	
	// Parse class types field
	ClassTypes = ManifestObject->GetStringField("ClassTypes");
	
	// Parse initial focus asset
	FocusAssetIdent = ManifestObject->GetStringField("FocusAsset");

	FString CategoryString = ManifestObject->GetStringField("Category");	
	if (CategoryString == "CodeFeature")
	{
		Category = EContentSourceCategory::CodeFeature;
	}
	else if (CategoryString == "BlueprintFeature")
	{
		Category = EContentSourceCategory::BlueprintFeature;
	}
	else if (CategoryString == "Content")
	{
		Category = EContentSourceCategory::Content;
	}
	else
	{
		Category = EContentSourceCategory::Unknown;
	}

	// Load image data
	FString IconFilename = ManifestObject->GetStringField("Thumbnail");
	TSharedPtr<TArray<uint8>> IconImageData = MakeShareable(new TArray<uint8>());
	FString ThumbnailFile = FPaths::Combine(*MountPoint, TEXT("Media"), *IconFilename);
	if( LoadPakFileToBuffer(PakPlatformFile, ThumbnailFile, *IconImageData) == true)
	{
		IconData = MakeShareable(new FImageData(IconFilename, IconImageData));
	}
	else
	{
		UE_LOG(LogFeaturePack, Warning, TEXT("Error in Feature pack %s. Cannot find thumbnail %s."), *InFeaturePackPath, *ThumbnailFile );
	}

	const TArray<TSharedPtr<FJsonValue>> ScreenshotFilenameArray = ManifestObject->GetArrayField("Screenshots");
	for (const TSharedPtr<FJsonValue> ScreenshotFilename : ScreenshotFilenameArray)
	{
		TSharedPtr<TArray<uint8>> SingleScreenshotData = MakeShareable(new TArray<uint8>);
		LoadPakFileToBuffer(PakPlatformFile, FPaths::Combine(*MountPoint, TEXT("Media"), *ScreenshotFilename->AsString()), *SingleScreenshotData);
		ScreenshotData.Add(MakeShareable(new FImageData(ScreenshotFilename->AsString(), SingleScreenshotData)));
	}
	bPackValid = true;
}

bool FFeaturePackContentSource::LoadPakFileToBuffer(FPakPlatformFile& PakPlatformFile, FString Path, TArray<uint8>& Buffer)
{
	bool bResult = false;
	TSharedPtr<IFileHandle> FileHandle(PakPlatformFile.OpenRead(*Path));
	if( FileHandle.IsValid())
	{
		Buffer.AddUninitialized(FileHandle->Size());
		bResult = FileHandle->Read(Buffer.GetData(), FileHandle->Size());
	}
	return bResult;
}

TArray<FLocalizedText> FFeaturePackContentSource::GetLocalizedNames()
{
	return LocalizedNames;
}

TArray<FLocalizedText> FFeaturePackContentSource::GetLocalizedDescriptions()
{
	return LocalizedDescriptions;
}

TArray<FLocalizedText> FFeaturePackContentSource::GetLocalizedAssetTypes()
{
	return LocalizedAssetTypesList;
}

FString FFeaturePackContentSource::GetClassTypesUsed()
{
	return ClassTypes;
}

EContentSourceCategory FFeaturePackContentSource::GetCategory()
{
	return Category;
}

TSharedPtr<FImageData> FFeaturePackContentSource::GetIconData()
{
	return IconData;
}

TArray<TSharedPtr<FImageData>> FFeaturePackContentSource::GetScreenshotData()
{
	return ScreenshotData;
}

bool FFeaturePackContentSource::InstallToProject(FString InstallPath)
{
	bool bResult = false;
	if( IsDataValid() == false )
	{
		UE_LOG(LogFeaturePack, Warning, TEXT("Trying to install invalid pack %s"), *InstallPath);
	}
	else
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		TArray<FString> AssetPaths;
		AssetPaths.Add(FeaturePackPath);

		TArray<UObject*> ImportedObjects = AssetToolsModule.Get().ImportAssets(AssetPaths, InstallPath );
		if( ImportedObjects.Num() == 0 )
		{
			UE_LOG(LogFeaturePack, Warning, TEXT("No objects imported installing pack %s"), *InstallPath);
		}
		else
		{
			// Save any imported assets.
			TArray<UPackage*> ToSave;
			for (auto ImportedObject : ImportedObjects)
			{
				ToSave.AddUnique(ImportedObject->GetOutermost());
			}
			FEditorFileUtils::PromptForCheckoutAndSave( ToSave, /*bCheckDirty=*/ false, /*bPromptToSave=*/ false );
			bResult = true;
			
			// Focus on a specific asset if we want to.
			if( GetFocusAssetName().IsEmpty() == false )
			{
				UObject* FocusAsset = LoadObject<UObject>(nullptr, *GetFocusAssetName());
				if (FocusAsset)
				{
					FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
					TArray<UObject*> SyncObjects;
					SyncObjects.Add(FocusAsset);
					ContentBrowserModule.Get().SyncBrowserToAssets(SyncObjects);
				}
			}
		}
	}
	return bResult;
}

FFeaturePackContentSource::~FFeaturePackContentSource()
{
}

bool FFeaturePackContentSource::IsDataValid() const
{
	if( bPackValid == false )
	{
		return false;
	}
	// To Do maybe validate other data here
	return true;	
}

FString FFeaturePackContentSource::GetFocusAssetName() const
{
	return FocusAssetIdent;
}
