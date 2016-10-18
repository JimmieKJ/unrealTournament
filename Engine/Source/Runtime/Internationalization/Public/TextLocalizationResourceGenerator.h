// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "LocTextHelper.h"

class FTextLocalizationResourceGenerator
{
public:
	/**
	 * Given a loc text helper, generate a compiled LocRes file for the given culture into the destination archive.
	 */
	INTERNATIONALIZATION_API static bool Generate(const FLocTextHelper& InLocTextHelper, const FString& InCultureToGenerate, const bool bSkipSourceCheck, FArchive& InDestinationArchive);

	/**
	 * Given a config file, generate a compiled LocRes file for the active culture and use it to update the live-entries in the localization manager.
	 */
	INTERNATIONALIZATION_API static bool GenerateAndUpdateLiveEntriesFromConfig(const FString& InConfigFilePath, const bool bSkipSourceCheck);

private:

	struct FLocalizationEntryTracker
	{
		struct FEntry
		{
			uint32 SourceStringHash;
			FString LocalizedString;
		};

		typedef TArray<FEntry> FEntryArray;


		struct FKeyTableKeyFuncs : BaseKeyFuncs<FEntryArray, FString, false>
		{
			static FORCEINLINE const FString& GetSetKey(const TPair<FString,FEntryArray>& Element)
			{
				return Element.Key;
			}
			static FORCEINLINE bool Matches(const FString& A, const FString& B)
			{
				return A.Equals( B, ESearchCase::CaseSensitive );
			}
			static FORCEINLINE uint32 GetKeyHash(const FString& Key)
			{
				return FCrc::StrCrc32<TCHAR>(*Key);
			}
		};
		typedef TMap<FString, FEntryArray, FDefaultSetAllocator, FKeyTableKeyFuncs> FKeyTable;


		struct FNamespaceTableKeyFuncs : BaseKeyFuncs<FKeyTable, FString, false>
		{
			static FORCEINLINE const FString& GetSetKey(const TPair<FString,FKeyTable>& Element)
			{
				return Element.Key;
			}
			static FORCEINLINE bool Matches(const FString& A, const FString& B)
			{
				return A.Equals( B, ESearchCase::CaseSensitive );
			}
			static FORCEINLINE uint32 GetKeyHash(const FString& Key)
			{
				return FCrc::StrCrc32<TCHAR>(*Key);
			}
		};
		typedef TMap<FString, FKeyTable, FDefaultSetAllocator, FNamespaceTableKeyFuncs> FNamespaceTable;

		FNamespaceTable Namespaces;

		void ReportCollisions() const;
		bool WriteToArchive(FArchive* const Archive);
	};
};