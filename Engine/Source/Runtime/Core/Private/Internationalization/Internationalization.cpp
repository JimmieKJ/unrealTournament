// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "TextCache.h"

#if UE_ENABLE_ICU
#include "ICUInternationalization.h"
#else
#include "LegacyInternationalization.h"
#endif

#define LOCTEXT_NAMESPACE "Internationalization"

FInternationalization* FInternationalization::Instance;

FInternationalization& FInternationalization::Get()
{
	if(!Instance)
	{
		Instance = new FInternationalization();
	}
	if(Instance && !Instance->IsInitialized())
	{
		Instance->Initialize();
	}
	return *Instance;
}

bool FInternationalization::IsAvailable()
{
	return Instance && Instance->IsInitialized();
}

void FInternationalization::TearDown()
{
	if (Instance && Instance->IsInitialized())
	{
		Instance->Terminate();
		FTextCache::Get().Flush();
	}
}

FText FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey)
{
	return FTextCache::Get().FindOrCache(InTextLiteral, InNamespace, InKey);
}

bool FInternationalization::SetCurrentCulture(const FString& Name)
{
	return Implementation->SetCurrentCulture(Name);
}

FCulturePtr FInternationalization::GetCulture(const FString& Name)
{
	return Implementation->GetCulture(Name);
}

void FInternationalization::Initialize()
{
	static bool IsInitializing = false;

	if(IsInitialized() || IsInitializing)
	{
		return;
	}
	struct FInitializingGuard
	{
		FInitializingGuard()	{IsInitializing = true;}
		~FInitializingGuard()	{IsInitializing = false;}
	} InitializingGuard;

	bIsInitialized = Implementation->Initialize();
}

void FInternationalization::Terminate()
{
	DefaultCulture.Reset();
	InvariantCulture.Reset();
	CurrentCulture.Reset();

	Implementation->Terminate();

	bIsInitialized = false;
	delete Instance;
	Instance = nullptr;
}

#if ENABLE_LOC_TESTING
namespace
{
	bool LeetifyInRange(FString& String, const int32 Begin, const int32 End)
	{
		bool Succeeded = false;
		for(int32 Index = Begin; Index < End; ++Index)
		{
			switch(String[Index])
			{
			case TEXT('A'): { String[Index] = TEXT('4'); Succeeded = true; } break;
			case TEXT('a'): { String[Index] = TEXT('@'); Succeeded = true; } break;
			case TEXT('B'): { String[Index] = TEXT('8'); Succeeded = true; } break;
			case TEXT('b'): { String[Index] = TEXT('8'); Succeeded = true; } break;
			case TEXT('E'): { String[Index] = TEXT('3'); Succeeded = true; } break;
			case TEXT('e'): { String[Index] = TEXT('3'); Succeeded = true; } break;
			case TEXT('G'): { String[Index] = TEXT('9'); Succeeded = true; } break;
			case TEXT('g'): { String[Index] = TEXT('9'); Succeeded = true; } break;
			case TEXT('I'): { String[Index] = TEXT('1'); Succeeded = true; } break;
			case TEXT('i'): { String[Index] = TEXT('!'); Succeeded = true; } break;
			case TEXT('O'): { String[Index] = TEXT('0'); Succeeded = true; } break;
			case TEXT('o'): { String[Index] = TEXT('0'); Succeeded = true; } break;
			case TEXT('S'): { String[Index] = TEXT('5'); Succeeded = true; } break;
			case TEXT('s'): { String[Index] = TEXT('$'); Succeeded = true; } break;
			case TEXT('T'): { String[Index] = TEXT('7'); Succeeded = true; } break;
			case TEXT('t'): { String[Index] = TEXT('7'); Succeeded = true; } break;
			case TEXT('Z'): { String[Index] = TEXT('2'); Succeeded = true; } break;
			case TEXT('z'): { String[Index] = TEXT('2'); Succeeded = true; } break;
			}
		}

		return Succeeded;
	}
}

FString& FInternationalization::Leetify(FString& SourceString)
{
	// Check that the string hasn't already been Leetified
	if( SourceString.IsEmpty() ||
		(SourceString[ 0 ] != 0x2021 && SourceString.Find( TEXT("\x00AB"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 0 ) == -1) )
	{
		bool Succeeded = false;

		FString OpenBlock = TEXT("{");
		FString CloseBlock = TEXT("}");
		uint32 SanityLoopCheck=0xFFFF;

		int32 CurrentBlockBeginPos=-1;
		int32 CurrentBlockEndPos=0;
		int32 PrevBlockBeginPos=0;
		int32 PrevBlockEndPos=-1;
		int32 CurrArgBlock=0;

		struct FBlockRange
		{
			int32 BeginPos;
			int32 EndPos;
		};

		TArray<FBlockRange> BlockRanges;
		while(--SanityLoopCheck > 0)
		{
			//Find the start of the next block. Delimited with an open brace '{'
			++CurrentBlockBeginPos;
			while(true)
			{
				CurrentBlockBeginPos = SourceString.Find(OpenBlock,ESearchCase::CaseSensitive, ESearchDir::FromStart, CurrentBlockBeginPos);
				if( CurrentBlockBeginPos == -1 )
				{
					break;//No block open started so we've reached the end of the format string.
				}

				if( CurrentBlockBeginPos >= 0 && SourceString[CurrentBlockBeginPos+1] == OpenBlock[0] )
				{
					//Skip past {{ literals in the format
					CurrentBlockBeginPos += 2;
					continue;
				}

				break;
			}

			//No more block opening braces found so we're done.
			if( CurrentBlockBeginPos == -1 )
			{
				break;
			}

			//Find the end of the block. Delimited with a close brace '}'
			CurrentBlockEndPos=SourceString.Find(CloseBlock,ESearchCase::CaseSensitive, ESearchDir::FromStart,CurrentBlockBeginPos);

			FBlockRange NewRange = { CurrentBlockBeginPos, CurrentBlockEndPos };
			BlockRanges.Add(NewRange);


			// Insertion of double arrows causes block start and end to be move later in the string, adjust for that.
			++CurrentBlockBeginPos;
			++CurrentBlockEndPos;

			Succeeded = LeetifyInRange(SourceString, PrevBlockEndPos + 1, CurrentBlockBeginPos) || Succeeded;

			PrevBlockBeginPos=CurrentBlockBeginPos;
			PrevBlockEndPos=CurrentBlockEndPos;

			++CurrArgBlock;
		}

		Succeeded = LeetifyInRange(SourceString, CurrentBlockEndPos + 1, SourceString.Len()) || Succeeded;

		// Insert double arrows around parameter blocks.
		if(BlockRanges.Num() > 0)
		{
			FString ResultString;
			int32 i;
			for(i = 0; i < BlockRanges.Num(); ++i)
			{
				// Append intermediate part of string.
				int32 EndOfLastPart = (i - 1 >= 0) ? (BlockRanges[i - 1].EndPos + 1) : 0;
				ResultString += SourceString.Mid(EndOfLastPart, BlockRanges[i].BeginPos - EndOfLastPart);
				// Wrap block.
				ResultString += TEXT("\x00AB");
				// Append block.
				ResultString += SourceString.Mid(BlockRanges[i].BeginPos, BlockRanges[i].EndPos - BlockRanges[i].BeginPos + 1);
				// Wrap block.
				ResultString += TEXT("\x00BB");
			}
			ResultString += SourceString.Mid(BlockRanges[i - 1].EndPos + 1, SourceString.Len() - BlockRanges[i - 1].EndPos + 1);
			SourceString = ResultString;
		}

		if( !Succeeded )
		{
			// Failed to LEETify, add something to beginning and end just to help mark the string.
			SourceString = FString(TEXT("\x2021")) + SourceString + FString(TEXT("\x2021"));
		}
	}

	return SourceString;
}
#endif

void FInternationalization::LoadAllCultureData()
{
	Implementation->LoadAllCultureData();
}

void FInternationalization::GetCultureNames(TArray<FString>& CultureNames) const
{
	Implementation->GetCultureNames(CultureNames);
}

TArray<FString> FInternationalization::GetPrioritizedCultureNames(const FString& Name)
{
	return Implementation->GetPrioritizedCultureNames(Name);
}

void FInternationalization::GetCulturesWithAvailableLocalization(const TArray<FString>& InLocalizationPaths, TArray< FCultureRef >& OutAvailableCultures, const bool bIncludeDerivedCultures)
{
	OutAvailableCultures.Reset();

	TArray<FString> AllLocalizationFolders;
	IFileManager& FileManager = IFileManager::Get();
	for(const auto& LocalizationPath : InLocalizationPaths)
	{
		/* Visitor class used to enumerate directories of culture */
		class FCultureEnumeratorVistor : public IPlatformFile::FDirectoryVisitor
		{
		public:
			FCultureEnumeratorVistor( TArray<FString>& OutLocalizationFolders )
				: LocalizationFolders(OutLocalizationFolders)
			{
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if(bIsDirectory)
				{
					// UE localization resource folders use "en-US" style while ICU uses "en_US"
					const FString LocalizationFolder = FPaths::GetCleanFilename(FilenameOrDirectory);
					const FString CanonicalName = FCulture::GetCanonicalName(LocalizationFolder);
					LocalizationFolders.AddUnique(CanonicalName);
				}

				return true;
			}

			/** Array to fill with the names of the UE localization folders available at the given path */
			TArray<FString>& LocalizationFolders;
		};

		FCultureEnumeratorVistor CultureEnumeratorVistor(AllLocalizationFolders);
		FileManager.IterateDirectory(*LocalizationPath, CultureEnumeratorVistor);
	}

	// Find any cultures that are a partial match for those we have translations for.
	if(bIncludeDerivedCultures)
	{
		TArray<FString> CultureNames;
		GetCultureNames(CultureNames);
		for(const FString& CultureName : CultureNames)
		{
			FCulturePtr Culture = GetCulture(CultureName);
			if (Culture.IsValid())
			{
				TArray<FString> PrioritizedParentCultureNames = Culture->GetPrioritizedParentCultureNames();

				for (const FString& PrioritizedParentCultureName : PrioritizedParentCultureNames)
				{
					if(AllLocalizationFolders.Contains(PrioritizedParentCultureName))
					{
						OutAvailableCultures.AddUnique(Culture.ToSharedRef());
						break;
					}
				}
			}
		}
	}
	// Find any cultures that are a complete match for those we have translations for.
	else
	{
		for(const FString& LocalizationFolder : AllLocalizationFolders)
		{
			FCulturePtr Culture = GetCulture(LocalizationFolder);
			if(Culture.IsValid())
			{
				OutAvailableCultures.AddUnique(Culture.ToSharedRef());
			}
		}
	}

	// Remove any cultures that were explicitly disabled
	OutAvailableCultures.RemoveAll([&](const FCultureRef& InCulture) -> bool
	{
		return Implementation->IsCultureDisabled(InCulture->GetName());
	});
}

FInternationalization::FInternationalization()
	:	bIsInitialized(false)
	,	Implementation(this)
{

}

#undef LOCTEXT_NAMESPACE
