// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GameSubCatalogEditorPCH.h"
#include "MultiLocHelper.h"
#include "Internationalization/InternationalizationArchive.h"
#include "Internationalization/Text.h"
#include "Serialization/JsonInternationalizationArchiveSerializer.h"

FMultiLocHelper::FMultiLocHelper()
{
	// make a custom export callback to export text values from JSON
	TextExportCallback.BindLambda([this](UProperty* Property, const void* Value) {
		TSharedPtr<FJsonValue> Result;

		// see if this is a Text struct property
		UTextProperty *TextProperty = Cast<UTextProperty>(Property);
		if (TextProperty)
		{
			// ok, it is, we can cast it
			const FText& Text = *(const FText*)Value; // safe to cast it now

			// make an object with keys for each localization key
			TSharedPtr<FJsonObject> Out = MakeShareable(new FJsonObject());
			for (const FString& Culture : GetCultures())
			{
				FString LocalizedText = GetLocalizedString(Culture, Text);
				Out->SetStringField(Culture, LocalizedText);
			}

			// set this as our result
			Result = MakeShareable(new FJsonValueObject(Out));
		}

		return Result;
	});
}

bool FMultiLocHelper::LoadAllArchives(const TCHAR* BypassCulture)
{
	bool bSuccess = true;

	// find all localization archives in the content directory
	TArray<FString> LocalizationDirs;
	FString DirPath = FPaths::GameContentDir() / TEXT("Localization/Game/*");
	IFileManager::Get().FindFiles(LocalizationDirs, *DirPath, false, true);

	// try to load them all
	for (const FString& Culture : LocalizationDirs)
	{
		// build the culture file path
		FString FilePath = FPaths::GameContentDir() / TEXT("Localization/Game") / *Culture / TEXT("Game.archive");
		FilePath = FPaths::ConvertRelativePathToFull(FilePath);

		// make sure the game archive file exists (if it doesn't, don't count this as a failure, may just be a stray empty directory
		if (FPaths::FileExists(FilePath))
		{
			// try to load the archive
			if (!LoadArchive(Culture, FilePath))
			{
				UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Unable to load archive for localization culture '%s'"), *Culture);
				bSuccess = false;
			}
		}
	}

	// if we didn't load any, fall back to bypass
	if (GetCultures().Num() <= 0)
	{
		if (BypassCulture != nullptr)
		{
			// try to load the bypass culture (just exports the existing FTexts as though they were this culture code)
			UE_LOG(LogGameSubCatalogEditor, Warning, TEXT("No localization directory found at %s. Assuming %s"), *DirPath, BypassCulture);
			if (!LoadBypassCulture(FString(BypassCulture)))
			{
				UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Unable to load localization bypass culture '%s'"), BypassCulture);
				bSuccess = false;
			}
		}
		else
		{
			UE_LOG(LogGameSubCatalogEditor, Warning, TEXT("No localization directory found at %s"), *DirPath);
			bSuccess = false;
		}
	}

	return bSuccess;
}

bool FMultiLocHelper::LoadArchive(const FString& CultureCode, const FString& ArchivePath)
{
	// make sure this is a valid culture code
	FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureCode);
	if (!Culture.IsValid())
	{
		return false;
	}

	//read in file as string
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *ArchivePath))
	{
		return false;
	}

	// make an archive and try to deserialize it
	TSharedRef<FInternationalizationArchive> Archive = MakeShareable(new FInternationalizationArchive());
	FJsonInternationalizationArchiveSerializer Loader;
	if (!Loader.DeserializeArchive(FileContents, Archive))
	{
		return false;
	}

	// add it to our map (replaces any already loaded version
	Cultures.AddUnique(CultureCode);
	Archives.Add(CultureCode, Archive);

	// convert to 2-letter code
	FString TwoLetterCode = Culture->GetTwoLetterISOLanguageName();
	if (!TwoLetterCode.Equals(CultureCode, ESearchCase::IgnoreCase))
	{
		// make sure something else hasn't already populated the 2 letter code
		if (!Archives.Contains(TwoLetterCode))
		{
			// add this archive
			Cultures.AddUnique(TwoLetterCode);
			Archives.Add(TwoLetterCode, Archive);
		}
	}

	return true;
}

bool FMultiLocHelper::LoadBypassCulture(const FString& CultureCode)
{
	// make sure this is a valid culture code
	FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureCode);
	if (!Culture.IsValid())
	{
		return false;
	}

	// add it to our map (replaces any already loaded version
	Cultures.AddUnique(CultureCode);

	// convert to 2-letter code
	FString TwoLetterCode = Culture->GetTwoLetterISOLanguageName();
	if (!TwoLetterCode.Equals(CultureCode, ESearchCase::IgnoreCase))
	{
		// add this code
		Cultures.AddUnique(TwoLetterCode);
	}

	return true;
}

FString FMultiLocHelper::GetLocalizedString(const FString& CultureCode, const FText& Text) const
{
	// get the namespace and key
	const FString* Source = FTextInspector::GetSourceString(Text);
	if (Source)
	{
		// find the archive for this code
		const TSharedRef<FInternationalizationArchive>* ArchivePtr = Archives.Find(CultureCode);
		if (ArchivePtr)
		{
			const TOptional<FString> Namespace = FTextInspector::GetNamespace(Text);
			const FInternationalizationArchive& Archive = *(*ArchivePtr);
			const FLocItem SearchSource(*Source);
			TSharedPtr< FArchiveEntry > Entry = Archive.FindEntryBySource(Namespace.Get(TEXT("")), SearchSource, nullptr);
			if (Entry.IsValid())
			{
				const FString& FinalText = Entry->Translation.Text;
				if (!FinalText.IsEmpty())
				{
					return FinalText;
				}
			}
		}

		// no localization, use source (we may have loaded bypass culture)
		return *Source;
	}

	// build an error localization
	return FString::Printf(TEXT("??%s/NoLocalization"), *CultureCode);
}
