// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Json.h"
#include "InternationalizationManifest.h"
#include "InternationalizationArchive.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "JsonInternationalizationArchiveSerializer.h"
#include "BreakIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateTextLocalizationReportCommandlet, Log, All);

UGenerateTextLocalizationReportCommandlet::UGenerateTextLocalizationReportCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGenerateTextLocalizationReportCommandlet::CountWords( const FString& Text ) const
{
	int32 NumOfWords = 0;
	// Calculate # of words

	TSharedRef<IBreakIterator> LineBreakIterator = FBreakIterator::CreateLineBreakIterator();
	LineBreakIterator->SetString( Text );

	int32 PreviousBreak = 0;
	int32 CurrentBreak = 0;

	while( ( CurrentBreak = LineBreakIterator->MoveToNext() ) != INDEX_NONE )
	{
		if ( CurrentBreak > PreviousBreak )
		{
			++NumOfWords;
		}
		PreviousBreak = CurrentBreak;
	}

	return NumOfWords;
}

int32 UGenerateTextLocalizationReportCommandlet::Main(const FString& Params)
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Set config file.
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	// Set config section.
	ParamVal = ParamVals.Find(FString(TEXT("Section")));

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// Common settings
	FString SourcePath;
	FString DestinationPath;

	// Settings for generating/appending to word count report file
	bool bWordCountReport = false;

	// Settings for generating loc conflict report file
	bool bConflictReport = false;
	
	// Get source path.
	if( !( GetPathFromConfig( *SectionName, TEXT("SourcePath"), SourcePath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Get destination path.
	if( !( GetPathFromConfig( *SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No destination path specified."));
		return -1;
	}

	// Get the timestamp from the commandline, if not provided we will use the current time.
	const FString* TimeStampParamVal = ParamVals.Find(FString(TEXT("TimeStamp")));
	if ( TimeStampParamVal && !TimeStampParamVal->IsEmpty() )
	{
		CmdlineTimeStamp = *TimeStampParamVal;
	}

	GetBoolFromConfig( *SectionName, TEXT("bWordCountReport"), bWordCountReport, GatherTextConfigPath );
	GetBoolFromConfig( *SectionName, TEXT("bConflictReport"), bConflictReport, GatherTextConfigPath );


	if( bWordCountReport )
	{
		if( !ProcessWordCountReport( SourcePath, DestinationPath ) )
		{
			UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("Failed to generate word count report."));
			return -1;
		}
	}

	if( bConflictReport )
	{
		if( !ProcessConflictReport( DestinationPath ) )
		{
			UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("Failed to generate localization conflict report."));
			return -1;
		}
	}

	return 0;
}

bool UGenerateTextLocalizationReportCommandlet::ProcessWordCountReport(const FString& SourcePath, const FString& DestinationPath)
{
	FString ManifestName;
	FString WordCountReportName;
	FString TimeStamp;
	TArray<FString> CulturesToGenerate;

	TimeStamp = (CmdlineTimeStamp.IsEmpty()) ? *FDateTime::Now().ToString() : CmdlineTimeStamp;

	// Get manifest name.
	if( !( GetStringFromConfig( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No manifest name specified."));
		return false;
	}

	// Get report name.
	if( !( GetStringFromConfig( *SectionName, TEXT("WordCountReportName"), WordCountReportName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No word count report name specified."));
		return false;
	}

	// Get cultures to generate.
	GetStringArrayFromConfig( *SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, GatherTextConfigPath );

	for(int32 i = 0; i < CulturesToGenerate.Num(); ++i)
	{
		if( FInternationalization::Get().GetCulture( CulturesToGenerate[i] ).IsValid() )
		{
			UE_LOG(LogGenerateTextLocalizationReportCommandlet, Verbose, TEXT("Specified culture is not a valid runtime culture, but may be a valid base language: %s"), *(CulturesToGenerate[i]));
		}
	}

	FString ReportStr;
	FString ReportFilePath = (DestinationPath / WordCountReportName);

	if ( FPaths::FileExists( ReportFilePath ) )
	{
		if ( !FFileHelper::LoadFileToString( ReportStr, *ReportFilePath ) )
		{
			UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("Unable to load report found at %s."), *ReportFilePath);
			return false;
		}
	}

	FWordCountReportData ReportData;
	if( !ReportData.FromCSV(ReportStr) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("Unable to read report data found at %s."), *ReportFilePath);
		return false;
	}

	// Read the manifest file from the source path.
	FString ManifestFilePath = (SourcePath / ManifestName);
	ManifestFilePath = FPaths::ConvertRelativePathToFull(ManifestFilePath);
	TSharedPtr<FJsonObject> ManifestJSONObject = ReadJSONTextFile(ManifestFilePath);
	if( !(ManifestJSONObject.IsValid()) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No manifest found at %s."), *ManifestFilePath);
		return false;
	}

	TSharedRef<FInternationalizationManifest> InternationalizationManifest = MakeShareable( new FInternationalizationManifest );
	{
		FJsonInternationalizationManifestSerializer ManifestSerializer;
		ManifestSerializer.DeserializeManifest(ManifestJSONObject.ToSharedRef(), InternationalizationManifest);
	}

	// If we are starting a new report file we will add a heading row
	if(ReportData.GetRowCount() == 0)
	{
		ReportData.AddRow();
		ReportData.AddColumn();
		ReportData.AddColumn();
		ReportData.SetEntry(0, 0, FWordCountReportData::ColHeadingDateTime);
		ReportData.SetEntry(0, 1, FWordCountReportData::ColHeadingWordCount);
	}

	int32 NewRowIndex = ReportData.AddRow();
	ReportData.SetEntry(NewRowIndex, FWordCountReportData::ColHeadingDateTime, TimeStamp);

	int32 TotalNumOfWords = 0;


	struct FCaseSensitiveStringKeyFuncs : BaseKeyFuncs<FString, FString, false>
	{
		static FORCEINLINE const FString& GetSetKey(const FString& Element)
		{
			return Element;
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

	TSet< FString, FCaseSensitiveStringKeyFuncs > CountedText;
	for(TManifestEntryBySourceTextContainer::TConstIterator i = InternationalizationManifest->GetEntriesBySourceTextIterator(); i; ++i)
	{
		// Gather relevant info from manifest entry.
		const TSharedRef<FManifestEntry>& ManifestEntry = i.Value();
		const FLocItem& Source = ManifestEntry->Source;

		bool HasNonOptional = false;
		for( auto ContextIter = ManifestEntry->Contexts.CreateConstIterator(); ContextIter; ++ContextIter )
		{
			if( !(*ContextIter).bIsOptional )
			{
				HasNonOptional = true;
				break;
			}
		}

		if( HasNonOptional )
		{
			const FString CountedTextId = Source.Text + FString(TEXT("::")) + ManifestEntry->Namespace;
			if ( !CountedText.Contains( CountedTextId ) )
			{
				TotalNumOfWords += CountWords( Source.Text );

				bool IsAlreadySet = false;
				CountedText.Add( CountedTextId, &IsAlreadySet );
				check( !IsAlreadySet );
			}
		}
	}

	ReportData.SetEntry(NewRowIndex, FWordCountReportData::ColHeadingWordCount, FString::FromInt( TotalNumOfWords ) );
	
	// For each culture:
	for(int32 Culture = 0; Culture < CulturesToGenerate.Num(); Culture++)
	{
		uint32 TranslatedWords = 0;
		FString CultureStr = CulturesToGenerate[Culture];
		FString CulturePath = SourcePath / (*CultureStr);
		CountedText.Empty();

		// Find archives in the culture-specific folder.
		TArray<FString> ArchiveFileNames;
		IFileManager::Get().FindFiles(ArchiveFileNames, *(CulturePath / TEXT("*.archive")), true, false);

		// For each archive:
		for(int32 ArchiveIndex = 0; ArchiveIndex < ArchiveFileNames.Num(); ++ArchiveIndex)
		{
			const FString ArchiveName = ArchiveFileNames[ArchiveIndex];

			// Read each archive file from the culture-named directory in the source path.
			FString ArchiveFilePath = CulturePath / ArchiveName;
			ArchiveFilePath = FPaths::ConvertRelativePathToFull(ArchiveFilePath);
			TSharedPtr<FJsonObject> ArchiveJSONObject = ReadJSONTextFile(ArchiveFilePath);
			if( !(ArchiveJSONObject.IsValid()) )
			{
				UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No archive found at %s."), *ArchiveFilePath);
				continue;
			}

			TSharedRef<FInternationalizationArchive> InternationalizationArchive = MakeShareable( new FInternationalizationArchive );
			{
				FJsonInternationalizationArchiveSerializer ArchiveSerializer;
				ArchiveSerializer.DeserializeArchive(ArchiveJSONObject.ToSharedRef(), InternationalizationArchive);
			}

			// Finds all the manifest entries in the archive and adds the source text word count to the running total if there is a valid translation.
			for (TManifestEntryBySourceTextContainer::TConstIterator i = InternationalizationManifest->GetEntriesBySourceTextIterator(); i; ++i)
			{
				// Gather relevant info from manifest entry.
				const TSharedRef<FManifestEntry>& ManifestEntry = i.Value();
				const FString& Namespace = ManifestEntry->Namespace;
				const FLocItem& Source = ManifestEntry->Source;

				if (ManifestEntry->Contexts.Num() > 0)
				{
					TSharedPtr<FArchiveEntry> ArchiveEntry = InternationalizationArchive->FindEntryBySource(Namespace, Source, ManifestEntry->Contexts[0].KeyMetadataObj);
					if (ArchiveEntry.IsValid() && !ArchiveEntry->Translation.Text.IsEmpty())
					{
						bool HasNonOptional = false;
						for (auto ContextIter = ManifestEntry->Contexts.CreateConstIterator(); ContextIter; ++ContextIter)
						{
							if (!(*ContextIter).bIsOptional)
							{
								HasNonOptional = true;
								break;
							}
						}

						const FString CountedTextId = Source.Text + FString(TEXT("::")) + ManifestEntry->Namespace;
						if (!CountedText.Contains(CountedTextId))
						{
							TranslatedWords += CountWords(Source.Text);

							bool IsAlreadySet = false;
							CountedText.Add(CountedTextId, &IsAlreadySet);
							check(!IsAlreadySet);
						}
					}
				}
			}
		}
		ReportData.SetEntry(NewRowIndex, CultureStr, FString::FromInt( TranslatedWords ) );
	}

	if( SourceControlInfo.IsValid() )
	{
		FText SCCErrorText;
		if (!SourceControlInfo->CheckOutFile(ReportFilePath, SCCErrorText))
		{
			UE_LOG(LogGenerateTextLocalizationReportCommandlet, Warning, TEXT("Check out of file %s failed: %s"), *ReportFilePath, *SCCErrorText.ToString());
		}
	}

	if ( !FFileHelper::SaveStringToFile( ReportData.ToCSV(), *ReportFilePath ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("Unable to save report at %s."), *ReportFilePath);
		return false;
	}
	return true;
}

bool UGenerateTextLocalizationReportCommandlet::ProcessConflictReport(const FString& DestinationPath)
{
	FString ConflictReportName;
	FString TimeStamp;


	TimeStamp = (CmdlineTimeStamp.IsEmpty()) ? *FDateTime::Now().ToString() : CmdlineTimeStamp;

	// Get report name.
	if( !( GetStringFromConfig( *SectionName, TEXT("ConflictReportName"), ConflictReportName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No conflict report name specified."));
		return false;
	}

	FString ReportStr;
	FString ReportFilePath = (DestinationPath / ConflictReportName);

	ReportStr = FConflictReportInfo::GetInstance().ToString();

	if( SourceControlInfo.IsValid() )
	{
		FText SCCErrorText;
		if (!SourceControlInfo->CheckOutFile(ReportFilePath, SCCErrorText))
		{
			UE_LOG(LogGenerateTextLocalizationReportCommandlet, Warning, TEXT("Check out of file %s failed: %s"), *ReportFilePath, *SCCErrorText.ToString());
		}
	}

	if ( !FFileHelper::SaveStringToFile( ReportStr, *ReportFilePath ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("Unable to save report at %s."), *ReportFilePath);
		return false;
	}
	return true;
}
