// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AddContentDialogPCH.h"
#include "FeaturePackContentSource.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IPlatformFilePak.h"
#include "FileHelpers.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
/*#include "SuperSearchModule.h"*/

#define LOCTEXT_NAMESPACE "ContentFeaturePacks"

DEFINE_LOG_CATEGORY_STATIC(LogFeaturePack, Log, All);

bool TryValidateTranslatedValue(TSharedPtr<FJsonValue> TranslatedValue, TSharedPtr<FString>& ErrorMessage)
{
	if (TranslatedValue.IsValid() == false)
	{
		ErrorMessage = MakeShareable(new FString("Invalid translated value"));
		return false;
	}

	const TSharedPtr<FJsonObject>* TranslatedObject;
	if (TranslatedValue->TryGetObject(TranslatedObject) == false)
	{
		ErrorMessage = MakeShareable(new FString("Invalid translated value"));
		return false;
	}

	if ((*TranslatedObject)->HasTypedField<EJson::String>("Language") == false)
	{
		ErrorMessage = MakeShareable(new FString("Translated value missing 'Language' field"));
		return false;
	}

	if ((*TranslatedObject)->HasTypedField<EJson::String>("Text") == false)
	{
		ErrorMessage = MakeShareable(new FString("Translated value missing 'Text' field"));
		return false;
	}

	return true;
}

bool TryValidateManifestObject(TSharedPtr<FJsonObject> ManifestObject, TSharedPtr<FString>& ErrorMessage)
{
	if (ManifestObject.IsValid() == false)
	{
		ErrorMessage = MakeShareable(new FString("Manifest object missing"));
		return false;
	}

	if (ManifestObject->HasTypedField<EJson::Array>("Name") == false)
	{
		ErrorMessage = MakeShareable(new FString("Manifest object missing 'Names' field"));
		return false;
	}

	for (TSharedPtr<FJsonValue> NameValue : ManifestObject->GetArrayField("Name"))
	{
		if (TryValidateTranslatedValue(NameValue, ErrorMessage) == false)
		{
			ErrorMessage = MakeShareable(new FString(FString::Printf(TEXT("Manifest object 'Names' field error: %s"), **ErrorMessage)));
		}
	}

	if (ManifestObject->HasTypedField<EJson::Array>("Description") == false)
	{
		ErrorMessage = MakeShareable(new FString("Manifest object missing 'Description' field"));
		return false;
	}

	for (TSharedPtr<FJsonValue> DescriptionValue : ManifestObject->GetArrayField("Description"))
	{
		if (TryValidateTranslatedValue(DescriptionValue, ErrorMessage) == false)
		{
			ErrorMessage = MakeShareable(new FString(FString::Printf(TEXT("Manifest object 'Description' field error: %s"), **ErrorMessage)));
		}
	}

	if (ManifestObject->HasTypedField<EJson::Array>("AssetTypes") == false)
	{
		ErrorMessage = MakeShareable(new FString("Manifest object missing 'AssetTypes' field"));
		return false;
	}

	for (TSharedPtr<FJsonValue> AssetTypesValue : ManifestObject->GetArrayField("AssetTypes"))
	{
		if (TryValidateTranslatedValue(AssetTypesValue, ErrorMessage) == false)
		{
			ErrorMessage = MakeShareable(new FString(FString::Printf(TEXT("Manifest object 'AssetTypes' field error: %s"), **ErrorMessage)));
		}
	}

	if (ManifestObject->HasTypedField<EJson::String>("ClassTypes") == false)
	{
		ErrorMessage = MakeShareable(new FString("Manifest object missing 'ClassTypes' field"));
		return false;
	}

	if (ManifestObject->HasTypedField<EJson::String>("Category") == false)
	{
		ErrorMessage = MakeShareable(new FString("Manifest object missing 'Category' field"));
		return false;
	}

	if (ManifestObject->HasTypedField<EJson::String>("Thumbnail") == false)
	{
		ErrorMessage = MakeShareable(new FString("Manifest object missing 'Thumbnail' field"));
		return false;
	}

	if (ManifestObject->HasTypedField<EJson::Array>("Screenshots") == false)
	{
		ErrorMessage = MakeShareable(new FString("Manifest object missing 'Screenshots' field"));
		return false;
	}

	return true;
}

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

	if (ManifestReader->GetErrorMessage().IsEmpty() == false)
	{
		UE_LOG(LogFeaturePack, Warning, TEXT("Error in Feature pack %s. Failed to parse manifest: %s"), *InFeaturePackPath, *ManifestReader->GetErrorMessage());
		Category = EContentSourceCategory::Unknown;
		return;
	}

	TSharedPtr<FString> ManifestObjectErrorMessage;
	if (TryValidateManifestObject(ManifestObject, ManifestObjectErrorMessage) == false)
	{
		UE_LOG(LogFeaturePack, Warning, TEXT("Error in Feature pack %s. Manifest object error: %s"), *InFeaturePackPath, **ManifestObjectErrorMessage);
		Category = EContentSourceCategory::Unknown;
		return;
	}

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
	
	// Parse asset types field
	if( ManifestObject->HasField("SearchTags")==true)
	{
		for (TSharedPtr<FJsonValue> AssetTypesValue : ManifestObject->GetArrayField("SearchTags"))
		{
			TSharedPtr<FJsonObject> LocalizedAssetTypesObject = AssetTypesValue->AsObject();
			LocalizedSearchTags.Add(FLocalizedTextArray(
				LocalizedAssetTypesObject->GetStringField("Language"),
				LocalizedAssetTypesObject->GetStringField("Text")));
		}
	}

	// Parse class types field
	ClassTypes = ManifestObject->GetStringField("ClassTypes");
	
	
	// Parse initial focus asset if we have one - this is not required
	if (ManifestObject->HasTypedField<EJson::String>("FocusAsset") == true)
	{
	FocusAssetIdent = ManifestObject->GetStringField("FocusAsset");
	}	

	// Use the path as the sort key - it will be alphabetical that way
	SortKey = FeaturePackPath;
	ManifestObject->TryGetStringField("SortKey", SortKey);

	FString CategoryString = ManifestObject->GetStringField("Category");	
	UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EContentSourceCategory"));
	int32 Index = Enum->FindEnumIndex(FName(*CategoryString));
	Category = Index != INDEX_NONE ? (EContentSourceCategory)Index : EContentSourceCategory::Unknown;

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

	FSuperSearchModule& SuperSearchModule = FModuleManager::LoadModuleChecked< FSuperSearchModule >(TEXT("SuperSearch"));
	SuperSearchModule.GetActOnSearchTextClicked().AddRaw(this, &FFeaturePackContentSource::HandleActOnSearchText);	
	SuperSearchModule.GetSearchTextChanged().AddRaw(this, &FFeaturePackContentSource::HandleSuperSearchTextChanged);
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

TArray<FLocalizedText> FFeaturePackContentSource::GetLocalizedNames() const
{
	return LocalizedNames;
}

TArray<FLocalizedText> FFeaturePackContentSource::GetLocalizedDescriptions() const
{
	return LocalizedDescriptions;
}

TArray<FLocalizedText> FFeaturePackContentSource::GetLocalizedAssetTypes() const
{
	return LocalizedAssetTypesList;
}


FString FFeaturePackContentSource::GetClassTypesUsed() const
{
	return ClassTypes;
}

EContentSourceCategory FFeaturePackContentSource::GetCategory() const
{
	return Category;
}

TSharedPtr<FImageData> FFeaturePackContentSource::GetIconData() const
{
	return IconData;
}

TArray<TSharedPtr<FImageData>> FFeaturePackContentSource::GetScreenshotData() const
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

FString FFeaturePackContentSource::GetSortKey() const
{
	return SortKey;
}

void FFeaturePackContentSource::HandleActOnSearchText(TSharedPtr<FSearchEntry> SearchEntry)
{
	if (SearchEntry.IsValid())
	{
		if (SearchEntry->bCategory == false)
		{
			UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EContentSourceCategory"));
			FString CurrentLanguage = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
			FLocalizedText CurrentName = ChooseLocalizedText(LocalizedNames,CurrentLanguage);
			FString MyTitle = FText::Format( LOCTEXT("FeaturePackSearchResult", "{0} ({1})"), CurrentName.GetText(), Enum->GetDisplayNameText((int32)Category)).ToString();
			if (SearchEntry->Title == MyTitle)
			{
				IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
				IAddContentDialogModule& AddContentDialogModule = FModuleManager::LoadModuleChecked<IAddContentDialogModule>("AddContentDialog");				
				AddContentDialogModule.ShowDialog(MainFrameModule.GetParentWindow().ToSharedRef());
			}
		}
	}
}

void FFeaturePackContentSource::TryAddFeaturePackCategory(FString CategoryTitle, TArray< TSharedPtr<FSearchEntry> >& OutSuggestions)
{
	if (OutSuggestions.ContainsByPredicate([&CategoryTitle](TSharedPtr<FSearchEntry>& InElement)
		{ return ((InElement->Title == CategoryTitle) && (InElement->bCategory == true)); }) == false)
	{
		TSharedPtr<FSearchEntry> FeaturePackCat = MakeShareable(new FSearchEntry());
		FeaturePackCat->bCategory = true;
		FeaturePackCat->Title = CategoryTitle;
		OutSuggestions.Add(FeaturePackCat);
	}
}

void FFeaturePackContentSource::HandleSuperSearchTextChanged(const FString& InText, TArray< TSharedPtr<FSearchEntry> >& OutSuggestions)
{
	FString FeaturePackSearchCat = LOCTEXT("FeaturePackSearchCategory", "Feature Packs").ToString();

	FString CurrentLanguage = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
	FLocalizedText CurrentName = ChooseLocalizedText(LocalizedNames,CurrentLanguage);
	FLocalizedTextArray CurrentTagSet = ChooseLocalizedTextArray(LocalizedSearchTags,CurrentLanguage);
	FText AsText = FText::FromString(InText);
	TArray<FText> TagArray = CurrentTagSet.GetTags();
	if (TagArray.Num() != 0)
	{
		UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EContentSourceCategory"));
		// Add a feature packs category		
		for (int32 iTag = 0; iTag < TagArray.Num(); iTag++)
		{
			if (TagArray[iTag].EqualToCaseIgnored(AsText))
			{
				// This will add the category if one doesnt exist
				TryAddFeaturePackCategory(FeaturePackSearchCat,OutSuggestions);
				TSharedPtr<FSearchEntry> FeaturePackEntry = MakeShareable(new FSearchEntry());
				FeaturePackEntry->Title = FText::Format( LOCTEXT("FeaturePackSearchResult", "{0} ({1})"), CurrentName.GetText(), Enum->GetDisplayNameText((int32)Category)).ToString();
				FeaturePackEntry->bCategory = false;
				OutSuggestions.Add(FeaturePackEntry);
				return;
			}
		}
	}
}

FLocalizedText FFeaturePackContentSource::ChooseLocalizedText(TArray<FLocalizedText> Choices, FString LanguageCode)
{
	FLocalizedText Default;
	for (const FLocalizedText& Choice : Choices)
	{
		if (Choice.GetTwoLetterLanguage() == LanguageCode)
		{
			return Choice;
			break;
		}
		else if (Choice.GetTwoLetterLanguage() == TEXT("en"))
		{
			Default = Choice;
		}
	}
	return Default;
}

FLocalizedTextArray FFeaturePackContentSource::ChooseLocalizedTextArray(TArray<FLocalizedTextArray> Choices, FString LanguageCode)
{
	FLocalizedTextArray Default;
	for (const FLocalizedTextArray& Choice : Choices)
	{
		if (Choice.GetTwoLetterLanguage() == LanguageCode)
		{
			return Choice;
			break;
		}
		else if (Choice.GetTwoLetterLanguage() == TEXT("en"))
		{
			Default = Choice;
		}
	}
	return Default;
}

#undef LOCTEXT_NAMESPACE 
