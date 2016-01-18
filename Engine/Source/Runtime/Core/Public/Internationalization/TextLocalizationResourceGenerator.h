// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

class FTextLocalizationResourceGenerator
{
public:
	CORE_API static bool Generate(const FString& SourcePath, const TSharedRef<FInternationalizationManifest>& InternationalizationManifest, const FString& NativeCulture, const FString& CultureToGenerate, FArchive* const DestinationArchive, IInternationalizationArchiveSerializer& ArchiveSerializer);

private:

	struct FLocalizationEntryTracker
	{
		struct FEntry
		{
			FString ArchiveName;
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