// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "BreakIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateTextLocalizationReportCommandlet, Log, All);

UGenerateTextLocalizationReportCommandlet::UGenerateTextLocalizationReportCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
	const FString TimeStamp = (CmdlineTimeStamp.IsEmpty()) ? *FDateTime::Now().ToString() : CmdlineTimeStamp;

	// Get manifest name.
	FString ManifestName;
	if( !( GetStringFromConfig( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No manifest name specified."));
		return false;
	}

	// Get archive name.
	FString ArchiveName;
	if( !( GetStringFromConfig( *SectionName, TEXT("ArchiveName"), ArchiveName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No archive name specified."));
		return false;
	}

	// Get report name.
	FString WordCountReportName;
	if( !( GetStringFromConfig( *SectionName, TEXT("WordCountReportName"), WordCountReportName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No word count report name specified."));
		return false;
	}

	// Get cultures to generate.
	TArray<FString> CulturesToGenerate;
	GetStringArrayFromConfig( *SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, GatherTextConfigPath );

	for(int32 i = 0; i < CulturesToGenerate.Num(); ++i)
	{
		if( FInternationalization::Get().GetCulture( CulturesToGenerate[i] ).IsValid() )
		{
			UE_LOG(LogGenerateTextLocalizationReportCommandlet, Verbose, TEXT("Specified culture is not a valid runtime culture, but may be a valid base language: %s"), *(CulturesToGenerate[i]));
		}
	}

	FString ReportStr;
	const FString ReportFilePath = (DestinationPath / WordCountReportName);

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

	// Load the manifest and all archives
	FLocTextHelper LocTextHelper(SourcePath, ManifestName, ArchiveName, FString(), CulturesToGenerate, MakeShareable(new FLocFileSCCNotifies(SourceControlInfo)));
	{
		FText LoadError;
		if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
		{
			UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("%s"), *LoadError.ToString());
			return false;
		}
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

	auto CountWords = [](const FString& InTextToCount) -> int32
	{
		int32 NumOfWords = 0;
		// Calculate # of words

		TSharedRef<IBreakIterator> LineBreakIterator = FBreakIterator::CreateLineBreakIterator();
		LineBreakIterator->SetString(InTextToCount);

		int32 PreviousBreak = 0;
		int32 CurrentBreak = 0;

		while ((CurrentBreak = LineBreakIterator->MoveToNext()) != INDEX_NONE)
		{
			if (CurrentBreak > PreviousBreak)
			{
				++NumOfWords;
			}
			PreviousBreak = CurrentBreak;
		}

		return NumOfWords;
	};

	int32 TotalNumOfWords = 0;
	TSet< FString, FCaseSensitiveStringKeyFuncs > CountedText;

	LocTextHelper.EnumerateSourceTexts([&TotalNumOfWords, &CountedText, &CountWords](TSharedRef<FManifestEntry> InManifestEntry) -> bool
	{
		const int32 NumWords = CountWords(InManifestEntry->Source.Text);

		// Gather relevant info from each manifest entry
		for (const FManifestContext& Context : InManifestEntry->Contexts)
		{
			if (!Context.bIsOptional)
			{
				const FString CountedTextId = FString::Printf(TEXT("%s::%s::%s"), *InManifestEntry->Source.Text, *InManifestEntry->Namespace, *Context.Key);
				if (!CountedText.Contains(CountedTextId))
				{
					TotalNumOfWords += NumWords;

					bool IsAlreadySet = false;
					CountedText.Add(CountedTextId, &IsAlreadySet);
					check(!IsAlreadySet);
				}
			}
		}

		return true; // continue enumeration
	}, true);

	ReportData.SetEntry(NewRowIndex, FWordCountReportData::ColHeadingWordCount, FString::FromInt( TotalNumOfWords ) );
	
	// For each culture:
	for (const FString& CultureName : CulturesToGenerate)
	{
		uint32 TranslatedWords = 0;
		CountedText.Reset();

		// Finds all the manifest entries in the archive and adds the source text word count to the running total if there is a valid translation.
		LocTextHelper.EnumerateSourceTexts([&LocTextHelper, &CultureName, &TranslatedWords, &CountedText, &CountWords](TSharedRef<FManifestEntry> InManifestEntry) -> bool
		{
			const int32 NumWords = CountWords(InManifestEntry->Source.Text);

			// Gather relevant info from each manifest entry
			for (const FManifestContext& Context : InManifestEntry->Contexts)
			{
				if (!Context.bIsOptional)
				{
					TSharedPtr<FArchiveEntry> ArchiveEntry = LocTextHelper.FindTranslation(CultureName, InManifestEntry->Namespace, Context.Key, Context.KeyMetadataObj);
					if (ArchiveEntry.IsValid() && ArchiveEntry->Source.IsExactMatch(InManifestEntry->Source) && !ArchiveEntry->Translation.Text.IsEmpty())
					{
						const FString CountedTextId = FString::Printf(TEXT("%s::%s::%s"), *InManifestEntry->Source.Text, *InManifestEntry->Namespace, *Context.Key);
						if (!CountedText.Contains(CountedTextId))
						{
							TranslatedWords += NumWords;

							bool IsAlreadySet = false;
							CountedText.Add(CountedTextId, &IsAlreadySet);
							check(!IsAlreadySet);
						}
					}
				}
			}
			
			return true; // continue enumeration
		}, true);

		ReportData.SetEntry(NewRowIndex, CultureName, FString::FromInt(TranslatedWords));
	}

	const bool bReportFileSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, ReportFilePath, [&ReportData](const FString& InSaveFileName) -> bool
	{
		return FFileHelper::SaveStringToFile(ReportData.ToCSV(), *InSaveFileName);
	});

	if (!bReportFileSaved)
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("Unable to save report at %s."), *ReportFilePath);
		return false;
	}

	return true;
}

bool UGenerateTextLocalizationReportCommandlet::ProcessConflictReport(const FString& DestinationPath)
{
	const FString TimeStamp = (CmdlineTimeStamp.IsEmpty()) ? *FDateTime::Now().ToString() : CmdlineTimeStamp;

	// Get report name.
	FString ConflictReportName;
	if (!GetStringFromConfig(*SectionName, TEXT("ConflictReportName"), ConflictReportName, GatherTextConfigPath))
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("No conflict report name specified."));
		return false;
	}

	const FString ReportStr = GatherManifestHelper->GetConflictReport();
	const FString ReportFilePath = (DestinationPath / ConflictReportName);

	const bool bReportFileSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, ReportFilePath, [&ReportStr](const FString& InSaveFileName) -> bool
	{
		return FFileHelper::SaveStringToFile(ReportStr, *InSaveFileName);
	});

	if (!bReportFileSaved)
	{
		UE_LOG(LogGenerateTextLocalizationReportCommandlet, Error, TEXT("Unable to save report at %s."), *ReportFilePath);
		return false;
	}

	return true;
}
