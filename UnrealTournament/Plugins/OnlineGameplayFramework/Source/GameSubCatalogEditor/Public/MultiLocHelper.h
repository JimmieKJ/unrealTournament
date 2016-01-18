// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FInternationalizationArchive;
class UProperty;
class FJsonValue;

class GAMESUBCATALOGEDITOR_API FMultiLocHelper
{
public:
	FMultiLocHelper();

	// Re-declare delegate from FJsonObjectConverter so we don't have header dependency
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedPtr<FJsonValue>, CustomExportCallback, UProperty* /* Property */, const void* /* Value */);

	// Try to load all archives found in the content directory. If no localization data is present, use the BypassCulture (pass nullptr to disable this behavior)
	bool LoadAllArchives(const TCHAR* BypassCulture = TEXT("en-US"));

	// Load a localization archive with the given culture code
	bool LoadArchive(const FString& CultureCode, const FString& ArchivePath);

	// adds the current culture to the Cultures list but doesn't load an archive (this is for projects without localization set up yet)
	bool LoadBypassCulture(const FString& CultureCode);

	// Given an FText, get the localized FString out of the loaded archive for this culture code
	FString GetLocalizedString(const FString& CultureCode, const FText& Text) const;

	// Get all loaded cultures
	inline const TArray<FString>& GetCultures() const { return Cultures; }

	// Get an export callback suitable for passing to FJsonObjectConverter
	inline const CustomExportCallback& GetTextExportCb() const { return TextExportCallback; }

private:
	CustomExportCallback TextExportCallback;
	TArray<FString> Cultures;
	TMap<FString, TSharedRef<FInternationalizationArchive> > Archives;
};