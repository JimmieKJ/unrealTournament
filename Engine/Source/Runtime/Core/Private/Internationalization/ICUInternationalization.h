// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#if UE_ENABLE_ICU

#include <unicode/umachine.h>

// Linux needs to have those compiled statically at least until we settle on .so location for deployed/native builds
#define NEEDS_ICU_DLLS		(IS_PROGRAM || !IS_MONOLITHIC) && PLATFORM_DESKTOP && !PLATFORM_LINUX

class FICUInternationalization
{
public:
	FICUInternationalization(FInternationalization* const I18N);

	bool Initialize();
	void Terminate();

	bool IsCultureRemapped(const FString& Name, FString* OutMappedCulture);
	bool IsCultureDisabled(const FString& Name);

	bool SetCurrentCulture(const FString& Name);
	void GetCultureNames(TArray<FString>& CultureNames) const;
	TArray<FString> GetPrioritizedCultureNames(const FString& Name);
	FCulturePtr GetCulture(const FString& Name);

private:
#if NEEDS_ICU_DLLS
	void LoadDLLs();
	void UnloadDLLs();
#endif

	void InitializeAvailableCultures();
	void ConditionalInitializeCultureMappings();
	void ConditionalInitializeDisabledCultures();

	FCulturePtr FindOrMakeCulture(const FString& Name, const bool AllowDefaultFallback = false);

private:
	struct FICUCultureData
	{
		FString Name;
		FString LanguageCode;
		FString ScriptCode;
		FString CountryCode;

		bool operator==(const FICUCultureData& Other) const
		{
			return Name == Other.Name;
		}

		bool operator!=(const FICUCultureData& Other) const
		{
			return Name != Other.Name;
		}
	};

private:
	FInternationalization* const I18N;

	TArray< void* > DLLHandles;

	TArray<FICUCultureData> AllAvailableCultures;
	TMap<FString, int32> AllAvailableCulturesMap;
	TMap<FString, TArray<int32>> AllAvailableLanguagesToSubCulturesMap;

	bool bHasInitializedCultureMappings;
	TMap<FString, FString> CultureMappings;

	bool bHasInitializedDisabledCultures;
	TSet<FString> DisabledCultures;

	TMap<FString, FCultureRef> CachedCultures;
	FCriticalSection CachedCulturesCS;

	static UBool OpenDataFile(const void* context, void** fileContext, void** contents, const char* path);
	static void CloseDataFile(const void* context, void* const fileContext, void* const contents);

	// Tidy class for storing the count of references for an ICU data file and the file's data itself.
	struct FICUCachedFileData
	{
		FICUCachedFileData(const int64 FileSize);
		FICUCachedFileData(const FICUCachedFileData& Source);
		FICUCachedFileData(FICUCachedFileData&& Source);
		~FICUCachedFileData();

		uint32 ReferenceCount;
		void* Buffer;
	};

	// Map for associating ICU data file paths with cached file data, to prevent multiple copies of immutable ICU data files from residing in memory.
	TMap<FString, FICUCachedFileData> PathToCachedFileDataMap;
};

#endif