// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Internationalization/InternationalizationMetadata.h"
#include "Json.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "JsonInternationalizationMetadataSerializer.h"
#include "ISourceControlModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextCommandletBase, Log, All);

TSharedPtr<FConflictReportInfo> FConflictReportInfo::StaticConflictInstance;


//////////////////////////////////////////////////////////////////////////
//UGatherTextCommandletBase

UGatherTextCommandletBase::UGatherTextCommandletBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGatherTextCommandletBase::Initialize( const TSharedRef< FManifestInfo >& InManifestInfo, const TSharedPtr< FGatherTextSCC >& InSourceControlInfo )
{
	ManifestInfo = InManifestInfo;
	SourceControlInfo = InSourceControlInfo;
}

void UGatherTextCommandletBase::CreateCustomEngine(const FString& Params)
{
	GEngine = GEditor = NULL;//Force a basic default engine. 
}

TSharedPtr<FJsonObject> UGatherTextCommandletBase::ReadJSONTextFile(const FString& InFilePath)
{
	//read in file as string
	FString FileContents;
	if ( !FFileHelper::LoadFileToString(FileContents, *InFilePath) )
	{
		UE_LOG(LogGatherTextCommandletBase, Error,TEXT("Failed to load file %s."), *InFilePath);
		return NULL;
	}

	//parse as JSON
	TSharedPtr<FJsonObject> JSONObject;

	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( FileContents );

	if( !FJsonSerializer::Deserialize( Reader, JSONObject ) || !JSONObject.IsValid())
	{
		UE_LOG(LogGatherTextCommandletBase, Error,TEXT("Invalid JSON in file %s."), *InFilePath);
		UE_LOG(LogGatherTextCommandletBase, Error, TEXT("JSON Error: %s."), *Reader->GetErrorMessage());
		return NULL;
	}

	return JSONObject;
}

bool UGatherTextCommandletBase::WriteJSONToTextFile(TSharedPtr<FJsonObject> Output, const FString& Filename, TSharedPtr<FGatherTextSCC> SourceControl)
{
	bool WasSuccessful = true;

	// If the user specified a reference file - write the entries read from code to a ref file
	if ( !Filename.IsEmpty() && Output.IsValid() )
	{
		const bool DidFileExist = FPaths::FileExists(Filename);
		if (DidFileExist)
		{
			if( SourceControl.IsValid() )
			{
				FText SCCErrorText;
				if (!SourceControl->CheckOutFile(Filename, SCCErrorText))
				{
					UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Check out of file %s failed: %s"), *Filename, *SCCErrorText.ToString());
				}
			}
		}

		if( WasSuccessful )
		{
			//Print the JSON data out to the ref file.
			FString OutputString;
			TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create( &OutputString );
			FJsonSerializer::Serialize( Output.ToSharedRef(), Writer );

			if (!FFileHelper::SaveStringToFile(OutputString, *Filename, FFileHelper::EEncodingOptions::ForceUnicode))
			{
				UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Failed to write localization entries to file %s"), *Filename);
				WasSuccessful = false;
			}
		}

		if (!DidFileExist)
		{
			if( SourceControl.IsValid() )
			{
				FText SCCErrorText;
				if (!SourceControl->CheckOutFile(Filename, SCCErrorText))
				{
					UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Check out of file %s failed: %s"), *Filename, *SCCErrorText.ToString());
				}
			}
		}
	}
	else
	{
		WasSuccessful = false;
	}

	return WasSuccessful;
}

FString UGatherTextCommandletBase::MungeLogOutput( const FString& InString )
{
	if(InString.IsEmpty())
	{
		return InString;
	}

	FString ResultStr = InString.ReplaceCharWithEscapedChar();
	if(!GIsBuildMachine)
	{
		return ResultStr;
	}

	static TArray<FString> ErrorStrs;
	if(ErrorStrs.Num() == 0)
	{
		ErrorStrs.Add(FString(TEXT("Error")));
		ErrorStrs.Add(FString(TEXT("Failed")));
		ErrorStrs.Add(FString(TEXT("[BEROR]"))); 
		ErrorStrs.Add(FString(TEXT("Utility finished with exit code: -1"))); 
		ErrorStrs.Add(FString(TEXT("is not recognized as an internal or external command"))); 
		ErrorStrs.Add(FString(TEXT("Could not open solution: "))); 
		ErrorStrs.Add(FString(TEXT("Parameter format not correct"))); 
		ErrorStrs.Add(FString(TEXT("Another build is already started on this computer."))); 
		ErrorStrs.Add(FString(TEXT("Sorry but the link was not completed because memory was exhausted."))); 
		ErrorStrs.Add(FString(TEXT("simply rerunning the compiler might fix this problem"))); 
		ErrorStrs.Add(FString(TEXT("No connection could be made because the target machine actively refused"))); 
		ErrorStrs.Add(FString(TEXT("Internal Linker Exception:"))); 
		ErrorStrs.Add(FString(TEXT(": warning LNK4019: corrupt string table"))); 
		ErrorStrs.Add(FString(TEXT("Proxy could not update its cache"))); 
		ErrorStrs.Add(FString(TEXT("You have not agreed to the Xcode license agreements"))); 
		ErrorStrs.Add(FString(TEXT("Connection to build service terminated"))); 
		ErrorStrs.Add(FString(TEXT("cannot execute binary file"))); 
		ErrorStrs.Add(FString(TEXT("Invalid solution configuration"))); 
		ErrorStrs.Add(FString(TEXT("is from a previous version of this application and must be converted in order to build"))); 
		ErrorStrs.Add(FString(TEXT("This computer has not been authenticated for your account using Steam Guard"))); 
		ErrorStrs.Add(FString(TEXT("invalid name for SPA section"))); 
		ErrorStrs.Add(FString(TEXT(": Invalid file name, "))); 
		ErrorStrs.Add(FString(TEXT("The specified PFX file do not exist. Aborting"))); 
		ErrorStrs.Add(FString(TEXT("binary is not found. Aborting"))); 
		ErrorStrs.Add(FString(TEXT("Input file not found: "))); 
		ErrorStrs.Add(FString(TEXT("An exception occurred during merging:"))); 
		ErrorStrs.Add(FString(TEXT("Install the 'Microsoft Windows SDK for Windows 7 and .NET Framework 3.5 SP1'"))); 
		ErrorStrs.Add(FString(TEXT("is less than package's new version 0x"))); 
		ErrorStrs.Add(FString(TEXT("current engine version is older than version the package was originally saved with")));
		ErrorStrs.Add(FString(TEXT("exceeds maximum length")));
		ErrorStrs.Add(FString(TEXT("can't edit exclusive file already opened")));
	}

	for( int32 ErrStrIdx = 0; ErrStrIdx < ErrorStrs.Num(); ErrStrIdx++ )
	{
		FString& FindStr = ErrorStrs[ErrStrIdx];
		FString ReplaceStr = FString::Printf(TEXT("%s %s"), *FindStr.Left(1), *FindStr.RightChop(1));

		ResultStr.ReplaceInline(*FindStr, *ReplaceStr);
	}

	return ResultStr;
}


bool UGatherTextCommandletBase::GetBoolFromConfig( const TCHAR* Section, const TCHAR* Key, bool& OutValue, const FString& Filename )
{
	bool bSuccess = GConfig->GetBool( Section, Key, OutValue, Filename );
	
	if( !bSuccess )
	{
		bSuccess = GConfig->GetBool( TEXT("CommonSettings"), Key, OutValue, Filename );
	}
	return bSuccess;
}

bool UGatherTextCommandletBase::GetStringFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename )
{
	bool bSuccess = GConfig->GetString( Section, Key, OutValue, Filename );

	if( !bSuccess )
	{
		bSuccess = GConfig->GetString( TEXT("CommonSettings"), Key, OutValue, Filename );
	}
	return bSuccess;
}

bool UGatherTextCommandletBase::GetPathFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename )
{
	bool bSuccess = GetStringFromConfig( Section, Key, OutValue, Filename );

	if( bSuccess )
	{
		if (FPaths::IsRelative(OutValue))
		{
			if (!FPaths::GameDir().IsEmpty())
			{
				OutValue = FPaths::Combine( *( FPaths::GameDir() ), *OutValue );
			}
			else
			{
				OutValue = FPaths::Combine( *( FPaths::EngineDir() ), *OutValue );
			}
		}
	}
	return bSuccess;
}

int32 UGatherTextCommandletBase::GetStringArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename )
{
	int32 count = GConfig->GetArray( Section, Key, OutArr, Filename );

	if( count == 0 )
	{
		count = GConfig->GetArray( TEXT("CommonSettings"), Key, OutArr, Filename );
	}
	return count;
}

int32 UGatherTextCommandletBase::GetPathArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename )
{
	int32 count = GetStringArrayFromConfig( Section, Key, OutArr, Filename );

	for (int32 i = 0; i < count; ++i)
	{
		if (FPaths::IsRelative(OutArr[i]))
		{
			const FString ProjectBasePath = FPaths::GameDir().IsEmpty() ? FPaths::EngineDir() : FPaths::GameDir();
			OutArr[i] = FPaths::Combine( *ProjectBasePath, *OutArr[i] );
			OutArr[i] = FPaths::ConvertRelativePathToFull(OutArr[i]);
		}
		FPaths::CollapseRelativeDirectories(OutArr[i]);
	}
	return count;
}

bool FManifestInfo::AddManifestDependency( const FString& InManifestFilePath )
{
	if( ManifestDependenciesFilePaths.Contains( InManifestFilePath ) )
	{
		return true;
	}

	bool bSuccess = false;

	if( FPaths::FileExists( InManifestFilePath ) )
	{
		FJsonInternationalizationManifestSerializer ManifestSerializer;
		TSharedPtr< FJsonObject > LoadedManifestDependencyJsonObject = UGatherTextCommandletBase::ReadJSONTextFile( InManifestFilePath );

		TSharedRef< FInternationalizationManifest > LoadedManifestDependency = MakeShareable( new FInternationalizationManifest );
		bSuccess = ManifestSerializer.DeserializeManifest( LoadedManifestDependencyJsonObject.ToSharedRef(), LoadedManifestDependency );
		
		if( bSuccess )
		{
			ManifestDependencies.Add( LoadedManifestDependency );
			ManifestDependenciesFilePaths.Add( InManifestFilePath );
		}
		else
		{
			UE_LOG(LogGatherTextCommandletBase, Warning, TEXT("Could not load manifest dependency file %s"), *InManifestFilePath);
		}
	}
	else
	{
		bSuccess = false;
		UE_LOG(LogGatherTextCommandletBase, Warning, TEXT("Could not find manifest dependency file %s"), *InManifestFilePath);
	}

	return bSuccess;
}

bool FManifestInfo::AddManifestDependencies( const TArray< FString >& InManifestFilePaths )
{
	bool bSuccess = true;
	for( int32 i = 0; i < InManifestFilePaths.Num(); i++ )
	{
		FString FilePath = InManifestFilePaths[i];
		bSuccess &= AddManifestDependency( FilePath );
	}
	return bSuccess;
}


TSharedPtr< FManifestEntry > FManifestInfo::FindDependencyEntryByContext( const FString& Namespace, const FContext& Context, FString& OutFileName )
{
	TSharedPtr<FManifestEntry> DependencyEntry = NULL;
	OutFileName = TEXT("");

	for( int32 Idx = 0; Idx < ManifestDependencies.Num(); Idx++ )
	{
		DependencyEntry = ManifestDependencies[Idx]->FindEntryByContext( Namespace, Context );
		if( DependencyEntry.IsValid() )
		{
			OutFileName = ManifestDependenciesFilePaths[Idx];
			break;
		}
	}
	return DependencyEntry;
}

TSharedPtr< FManifestEntry > FManifestInfo::FindDependencyEntryBySource( const FString& Namespace, const FLocItem& Source, FString& OutFileName )
{
	TSharedPtr<FManifestEntry> DependencyEntry = NULL;
	OutFileName = TEXT("");

	for( int32 Idx = 0; Idx < ManifestDependencies.Num(); Idx++ )
	{
		DependencyEntry = ManifestDependencies[Idx]->FindEntryBySource( Namespace, Source );
		if( DependencyEntry.IsValid() )
		{
			OutFileName = ManifestDependenciesFilePaths[Idx];
			break;
		}
	}
	return DependencyEntry;
}

void FManifestInfo::ApplyManifestDependencies()
{
	if( ManifestDependencies.Num() > 0 )
	{
		TSharedRef< FInternationalizationManifest > NewManifest =  MakeShareable( new FInternationalizationManifest );
		// We'll generate a new manifest by only including items that are not in the dependencies
		for( TManifestEntryByContextIdContainer::TConstIterator It( Manifest->GetEntriesByContextIdIterator() ); It; ++It )
		{
			const TSharedRef<FManifestEntry> ManifestEntry = It.Value();

			for(  auto ContextIt = ManifestEntry->Contexts.CreateConstIterator(); ContextIt; ++ContextIt )
			{
				FString DependencyFileName;

				const TSharedPtr<FManifestEntry> DependencyEntry = FindDependencyEntryByContext( ManifestEntry->Namespace, *ContextIt, DependencyFileName );
				
				if( DependencyEntry.IsValid() )
				{
					if( !(DependencyEntry->Source.IsExactMatch( ManifestEntry->Source )) )
					{
						// There is a dependency manifest entry that has the same namespace and keys as our main manifest entry but the source text differs.
						FString Message = UGatherTextCommandletBase::MungeLogOutput( FString::Printf(TEXT("Found previously entered localized string [%s] %s %s=\"%s\" %s. It was previously \"%s\" %s in dependency manifest %s."),
							*ManifestEntry->Namespace,
							*ContextIt->Key,
							*FJsonInternationalizationMetaDataSerializer::MetadataToString( ContextIt->KeyMetadataObj ),
							*ManifestEntry->Source.Text,
							*FJsonInternationalizationMetaDataSerializer::MetadataToString(ManifestEntry->Source.MetadataObj),
							*DependencyEntry->Source.Text,
							*FJsonInternationalizationMetaDataSerializer::MetadataToString(DependencyEntry->Source.MetadataObj),
							*DependencyFileName));
						UE_LOG(LogGatherTextCommandletBase, Warning, TEXT("%s"), *Message);

						FConflictReportInfo::GetInstance().AddConflict( ManifestEntry->Namespace, ContextIt->Key, ContextIt->KeyMetadataObj, ManifestEntry->Source, *ContextIt->SourceLocation );

						FContext* ConflictingContext = DependencyEntry->FindContext( ContextIt->Key, ContextIt->KeyMetadataObj );
						FString DependencyEntryFullSrcLoc = ( !DependencyFileName.IsEmpty() ) ? DependencyFileName : ConflictingContext->SourceLocation;
						
						FConflictReportInfo::GetInstance().AddConflict( ManifestEntry->Namespace, ContextIt->Key, ContextIt->KeyMetadataObj, DependencyEntry->Source, DependencyEntryFullSrcLoc );

					}
				}
				else
				{
					// Since we did not find any entries in the dependencies list that match, we'll add to the new manifest
					bool bAddSuccessful = NewManifest->AddSource( ManifestEntry->Namespace, ManifestEntry->Source, *ContextIt );
					if(!bAddSuccessful)
					{
						UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Could not process localized string: %s [%s] %s=\"%s\" %s."),
							*ManifestEntry->Namespace,
							*ContextIt->Key,
							*ManifestEntry->Source.Text,
							*FJsonInternationalizationMetaDataSerializer::MetadataToString( ManifestEntry->Source.MetadataObj ));
					}
				}
			}
		}

		Manifest = NewManifest;
	}
}

bool FManifestInfo::AddEntry( const FString& EntryDescription, const FString& Namespace, const FLocItem& Source, const FContext& Context )
{
	bool bAddSuccessful = false;
	// Check if the entry already exists in the manifest or one of the manifest dependencies
	FString ExistingEntryFileName;
	TSharedPtr< FManifestEntry > ExistingEntry = Manifest->FindEntryByContext( Namespace, Context );
	if( !ExistingEntry.IsValid() )
	{
		ExistingEntry = FindDependencyEntryByContext( Namespace, Context, ExistingEntryFileName );
	}

	if( ExistingEntry.IsValid() )
	{
		if( Source.IsExactMatch( ExistingEntry->Source ) )
		{
			bAddSuccessful = true;
		}
		else
		{
			// Grab the source location of the conflicting context
			FContext* ConflictingContext = ExistingEntry->FindContext( Context.Key, Context.KeyMetadataObj );
			FString ExistingEntrySourceLocation = ( !ExistingEntryFileName.IsEmpty() ) ?  ExistingEntryFileName : ConflictingContext->SourceLocation;

			FString Message = UGatherTextCommandletBase::MungeLogOutput( FString::Printf(TEXT("Previously entered localized string: %s [%s] %s %s=\"%s\" %s. It was previously \"%s\" %s in %s." ),
				*EntryDescription,
				*Namespace,
				*Context.Key,
				*FJsonInternationalizationMetaDataSerializer::MetadataToString( Context.KeyMetadataObj ),
				*Source.Text,
				*FJsonInternationalizationMetaDataSerializer::MetadataToString( Source.MetadataObj ),
				*ExistingEntry->Source.Text,
				*FJsonInternationalizationMetaDataSerializer::MetadataToString( ExistingEntry->Source.MetadataObj ),
				*ExistingEntrySourceLocation));

			UE_LOG(LogGatherTextCommandletBase, Warning, TEXT("%s"), *Message);

			FConflictReportInfo::GetInstance().AddConflict(Namespace, Context.Key, Context.KeyMetadataObj, Source, Context.SourceLocation );
			FConflictReportInfo::GetInstance().AddConflict(Namespace, Context.Key, Context.KeyMetadataObj, ExistingEntry->Source, ExistingEntrySourceLocation);
		}
	}
	else
	{
		bAddSuccessful = Manifest->AddSource( Namespace, Source, Context );
		if( !bAddSuccessful )
		{
			UE_LOG(LogGatherTextCommandletBase, Error, TEXT("Could not process localized string: %s [%s] %s=\"%s\" %s."),
				*EntryDescription,
				*Namespace,
				*Context.Key,
				*Source.Text,
				*FJsonInternationalizationMetaDataSerializer::MetadataToString( Source.MetadataObj ));
		}
	}
	return bAddSuccessful;
}


const TCHAR* FWordCountReportData::EntryDelimiter = TEXT(",");
const TCHAR* FWordCountReportData::NewLineDelimiter = TEXT("\n");
const FString FWordCountReportData::ColHeadingDateTime = TEXT("Date/Time");
const FString FWordCountReportData::ColHeadingWordCount = TEXT("Word Count");

bool FWordCountReportData::HasHeader() const
{
	// No real good way to decide if there is a header so here we just check to see if the first character is a letter
	if( Data.Num() > 0 && Data[0].Num() > 0 && Data[0][0].Len() > 0 )
	{
		return FChar::IsAlpha( Data[0][0][0] );
	}
	return false;
}

int32 FWordCountReportData::GetColumnCount() const
{
	if( Data.Num() > 0 )
	{
		return Data[0].Num();
	}
	return 0;
}


int32 FWordCountReportData::GetRowCount() const
{
	if( Data.Num() > 0 )
	{
		return Data.Num();
	}
	return 0;
}

const FString* FWordCountReportData::GetColumnHeader(int32 ColumnIndex) const
{
	if( ColumnIndex >= 0 && HasHeader() && ColumnIndex < GetColumnCount() )
	{
		return &Data[0][ColumnIndex];
	}
	return NULL;
}

const FString* FWordCountReportData::GetEntry(int32 RowIndex, int32 ColumnIndex) const
{
	if( RowIndex >= 0 && ColumnIndex >= 0 && RowIndex < GetRowCount() && ColumnIndex < GetColumnCount() )
	{
		return &Data[RowIndex][ColumnIndex];
	}
	return NULL;
}

bool FWordCountReportData::FromCSV( const FString& InString )
{
	Data.Empty();

	if( InString.Len() == 0 )
	{
		// An empty input string is valid but there is nothing to process
		return true;
	}

	FString CSVString = InString.Replace(TEXT("\r\n"), NewLineDelimiter);
	// Note, we do a trivial parse here and assume every delimiter we see separates entries/rows and is not contained within an entry
	TArray<FString> CSVRows;

	int32 NumRows = CSVString.ParseIntoArray( CSVRows, NewLineDelimiter, true );

	for(int32 RowIdx = 0; RowIdx < NumRows; RowIdx++ )
	{
		TArray< FString > RowEntriesArray;
		CSVRows[RowIdx].ParseIntoArray( RowEntriesArray, EntryDelimiter, false);
		Data.Add(RowEntriesArray);
	}

	// Do some validation of the data
	NumRows = GetRowCount();
	int32 NumCols = GetColumnCount();
	for(int32 RowIdx = 1; RowIdx < NumRows; RowIdx++ )
	{
		// Make sure each row has the same number of column entries
		if( !ensureMsgf(NumCols == Data[RowIdx].Num(), TEXT("Row index %d has an invalid number of columns"), RowIdx) )
		{
			return false;
		}
	}

	return true;
}

FString FWordCountReportData::ToCSV() const
{
	FString OutputStr;
	for(int32 RowIdx = 0; RowIdx < Data.Num(); RowIdx++)
	{
		const TArray< FString >& Row = Data[RowIdx];
		for(int32 RowEntry = 0; RowEntry < Row.Num(); RowEntry++)
		{
			OutputStr += Row[RowEntry];
			if(RowEntry < Row.Num() - 1)
			{
				OutputStr += EntryDelimiter;
			}
		}
		if(RowIdx < Data.Num() - 1)
		{
			OutputStr += NewLineDelimiter;
		}
	}
	return OutputStr;
}

int32 FWordCountReportData::GetColumnIndex( const FString& ColumnHeading ) const
{
	if(HasHeader())
	{
		const TArray< FString >& Row = Data[0];
		return Row.Find(ColumnHeading);
	}
	return INDEX_NONE;
}

void FWordCountReportData::SetEntry( int32 RowIndex, int32 ColumnIndex, const FString& EntryString )
{
	if( RowIndex >= 0 && ColumnIndex >= 0 && RowIndex < GetRowCount() && ColumnIndex < GetColumnCount() )
	{
		Data[RowIndex][ColumnIndex] = EntryString;
	}
}

void FWordCountReportData::SetEntry( int32 RowIndex, const FString& ColumnHeading, const FString& EntryString )
{
	int32 ColumnIndex = GetColumnIndex(ColumnHeading);
	if(INDEX_NONE != ColumnIndex)
	{
		SetEntry(RowIndex, ColumnIndex, EntryString);
	}
	else
	{
		ColumnIndex = AddColumn(&ColumnHeading);
		SetEntry(RowIndex, ColumnIndex, EntryString);
	}

}

int32 FWordCountReportData::AddRow()
{
	TArray< FString > RowEntriesArray;
	for(int32 ColIndex = 0; ColIndex < GetColumnCount(); ColIndex++)
	{
		RowEntriesArray.Add(FString(TEXT("")));
	}
	return Data.Add(RowEntriesArray);
}

int32 FWordCountReportData::AddColumn(const FString* ColumnHeading )
{
	int32 RowIndex = 0;
	if( ColumnHeading != NULL && HasHeader() )
	{
		// Check to see if a column with the provided header already exists
		int32 ExistingColumnIndex = GetColumnIndex(*ColumnHeading);
		if(ExistingColumnIndex != INDEX_NONE)
		{
			return ExistingColumnIndex;
		}

		Data[0].Add(*ColumnHeading);
		RowIndex++;
	}

	for(;RowIndex < Data.Num(); RowIndex++)
	{
		Data[RowIndex].Add(FString(TEXT("")));
	}

	return GetColumnCount() - 1;
}

void FLocConflict::Add( const FLocItem& Source, const FString& SourceLocation )
{
	EntriesBySourceLocation.AddUnique( SourceLocation, Source );
}

FConflictReportInfo& FConflictReportInfo::GetInstance()
{
	if( !StaticConflictInstance.IsValid() )
	{
		StaticConflictInstance = MakeShareable( new FConflictReportInfo() );
	}
	return *StaticConflictInstance;
}

void FConflictReportInfo::DeleteInstance()
{
	StaticConflictInstance.Reset();
}

TSharedPtr< FLocConflict > FConflictReportInfo::FindEntryByKey( const FString& Namespace, const FString& Key, const TSharedPtr<FLocMetadataObject> KeyMetadata )
{
	TArray< TSharedRef< FLocConflict > > MatchingEntries;
	EntriesByKey.MultiFind( Key, MatchingEntries );

	for( int Idx = 0; Idx < MatchingEntries.Num(); Idx++ )
	{
		if( MatchingEntries[Idx]->Namespace == Namespace )
		{
			if( KeyMetadata.IsValid() != MatchingEntries[Idx]->KeyMetadataObj.IsValid() )
			{
				continue;
			}
			else if ( (!KeyMetadata.IsValid() && !MatchingEntries[Idx]->KeyMetadataObj.IsValid()) ||
				*(KeyMetadata) == *(MatchingEntries[Idx]->KeyMetadataObj) )
			{
				return MatchingEntries[Idx];
			}
		}
	}

	return NULL;
}

void FConflictReportInfo::AddConflict(const FString& Namespace, const FString& Key, const TSharedPtr<FLocMetadataObject> KeyMetadata, const FLocItem& Source, const FString& SourceLocation )
{
	TSharedPtr< FLocConflict > ExistingEntry = FindEntryByKey( Namespace, Key, KeyMetadata );
	if( !ExistingEntry.IsValid() )
	{
		TSharedRef< FLocConflict > NewEntry = MakeShareable( new FLocConflict( Namespace, Key, KeyMetadata ) );
		EntriesByKey.Add( Key, NewEntry );
		ExistingEntry = NewEntry;
	}
	ExistingEntry->Add( Source, SourceLocation.ReplaceCharWithEscapedChar() );
}

FString FConflictReportInfo::ToString()
{
	FString Report;
	for( auto ConflictIter = GetEntryConstIterator(); ConflictIter; ++ConflictIter )
	{
		const TSharedRef<FLocConflict>& Conflict = ConflictIter.Value();
		const FString& Namespace = Conflict->Namespace;
		const FString& Key = Conflict->Key;

		bool bAddToReport = false;
		TArray<FLocItem> SourceList; 
		Conflict->EntriesBySourceLocation.GenerateValueArray(SourceList);
		if(SourceList.Num() >= 2)
		{
			for(int32 i = 0; i < SourceList.Num() - 1 && !bAddToReport; ++i)
			{
				for(int32 j = i+1; j < SourceList.Num() && !bAddToReport; ++j)
				{
					if( !(SourceList[i] == SourceList[j]) )
					{
						bAddToReport = true;
					}
				}
			}
		}
		
		if(bAddToReport)
		{
			FString KeyMetadataString = FJsonInternationalizationMetaDataSerializer::MetadataToString( Conflict->KeyMetadataObj );
			Report += FString::Printf(TEXT("%s - %s %s\n"), *Namespace, *Key, *KeyMetadataString );
						
			for(auto EntryIter = Conflict->EntriesBySourceLocation.CreateConstIterator(); EntryIter; ++EntryIter)
			{
				const FString& SourceLocation = EntryIter.Key();
				FString ProcessedSourceLocation = FPaths::ConvertRelativePathToFull(SourceLocation);
				ProcessedSourceLocation.ReplaceInline( TEXT("\\"), TEXT("/"));
				ProcessedSourceLocation.ReplaceInline( *FPaths::RootDir(), TEXT("/"));

				const FString& SourceText = EntryIter.Value().Text.ReplaceCharWithEscapedChar();

				FString SourceMetadataString =  FJsonInternationalizationMetaDataSerializer::MetadataToString( EntryIter.Value().MetadataObj );
				Report += FString::Printf(TEXT("\t%s - \"%s\" %s\n"), *ProcessedSourceLocation, *SourceText, *SourceMetadataString);
			}
			Report += TEXT("\n");
		}
	}
	return Report;
}

FGatherTextSCC::FGatherTextSCC()
{
	ISourceControlModule::Get().GetProvider().Init();
}

FGatherTextSCC::~FGatherTextSCC()
{
	if( CheckedOutFiles.Num() > 0 )
	{
		UE_LOG(LogGatherTextCommandletBase, Log, TEXT("Source Control wrapper shutting down with checked out files."));
	}

	ISourceControlModule::Get().GetProvider().Close();
}

bool FGatherTextSCC::CheckOutFile(const FString& InFile, FText& OutError)
{
	if ( InFile.IsEmpty() || InFile.StartsWith(TEXT("\\\\")) )
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "InvalidFileSpecified", "Could not checkout file at invalid path.");
		return false;
	}

	FText SCCError;
	if( !IsReady( SCCError ) )
	{
		OutError = SCCError;
		return false;
	}

	FString AbsoluteFilename = FPaths::ConvertRelativePathToFull( InFile );

	if( CheckedOutFiles.Contains( AbsoluteFilename ) )
	{
		return true;
	}

	bool bSuccessfullyCheckedOut = false;
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add( AbsoluteFilename );

	FFormatNamedArguments Args;
	Args.Add(TEXT("Filepath"), FText::FromString(InFile));

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState( AbsoluteFilename, EStateCacheUsage::ForceUpdate );
	if(SourceControlState.IsValid())
	{
		FString Other;
		if( SourceControlState->IsAdded() ||
			SourceControlState->IsCheckedOut())
		{
			// Already checked out or opened for add
			bSuccessfullyCheckedOut = true;
		}
		else if(SourceControlState->CanCheckout())
		{
			bSuccessfullyCheckedOut = (SourceControlProvider.Execute( ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut ) == ECommandResult::Succeeded);
			if (!bSuccessfullyCheckedOut)
			{
				OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "FailedToCheckOutFile", "Failed to check out file '{Filepath}'."), Args);
			}
		}
		else if(!SourceControlState->IsSourceControlled() && SourceControlState->CanAdd())
		{
			bSuccessfullyCheckedOut = (SourceControlProvider.Execute( ISourceControlOperation::Create<FMarkForAdd>(), FilesToBeCheckedOut ) == ECommandResult::Succeeded);
			if (!bSuccessfullyCheckedOut)
			{
				OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "FailedToAddFileToSourceControl", "Failed to add file '{Filepath}' to source control."), Args);
			}
		}
		else if(!SourceControlState->IsCurrent())
		{
			OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "FileIsNotAtHeadRevision", "File '{Filepath}' is not at head revision."), Args);
		}
		else if(SourceControlState->IsCheckedOutOther(&(Other)))
		{
			Args.Add(TEXT("Username"), FText::FromString(Other));
			OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "FileIsAlreadyCheckedOutByAnotherUser", "File '{Filepath}' is checked out by another ('{Username}')."), Args);
		}
		else
		{
			// Improper or invalid SCC state
			OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "CouldNotGetStateOfFile", "Could not determine source control state of file '{Filepath}'."), Args);
		}
	}
	else
	{
		// Improper or invalid SCC state
		OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "CouldNotGetStateOfFile", "Could not determine source control state of file '{Filepath}'."), Args);
	}

	if( bSuccessfullyCheckedOut )
	{
		CheckedOutFiles.AddUnique(AbsoluteFilename);
	}

	return bSuccessfullyCheckedOut;
}

bool FGatherTextSCC::CheckinFiles( const FText& InChangeDescription, FText& OutError )
{
	if( CheckedOutFiles.Num() == 0 )
	{
		return true;
	}

	FText SCCError;
	if( !IsReady( SCCError ) )
	{
		OutError = SCCError;
		return false;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	SourceControlHelpers::RevertUnchangedFiles(SourceControlProvider, CheckedOutFiles);

	// remove unchanged files
	for (int32 VerifyIndex = CheckedOutFiles.Num()-1; VerifyIndex >= 0; --VerifyIndex)
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState( CheckedOutFiles[VerifyIndex], EStateCacheUsage::ForceUpdate );
		if (SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded())
		{
			CheckedOutFiles.RemoveAt(VerifyIndex);
		}
	}

	if (CheckedOutFiles.Num() > 0)
	{
		TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
		CheckInOperation->SetDescription(InChangeDescription);
		if (!SourceControlProvider.Execute( CheckInOperation, CheckedOutFiles ))
		{
			OutError = NSLOCTEXT("GatherTextCmdlet", "FailedToCheckInFiles", "The checked out localization files could not be checked in.");
			return false;
		}
		CheckedOutFiles.Empty();
	}
	return true;
}

bool FGatherTextSCC::CleanUp(FText& OutError)
{
	if( CheckedOutFiles.Num() == 0 )
	{
		return true;
	}

	bool bCleanupSuccess = true;
	FString AccumulatedErrorsStr;
	const int32 FileCount = CheckedOutFiles.Num();
	int32 FileIdx = 0;
	for( int32 i = 0; i < FileCount; ++i )
	{
		FText ErrorText;
		bool bReverted = RevertFile(CheckedOutFiles[FileIdx], ErrorText);
		bCleanupSuccess &= bReverted;

		if( !bReverted )
		{
			if( !AccumulatedErrorsStr.IsEmpty() )
			{
				AccumulatedErrorsStr += TEXT(", ");
			}
			AccumulatedErrorsStr += FString::Printf(TEXT("%s : %s"), *CheckedOutFiles[FileIdx], *ErrorText.ToString());
			++FileIdx;
		}
	}

	if( !bCleanupSuccess )
	{
		OutError = FText::Format(NSLOCTEXT("GatherTextCmdlet", "CouldNotCompleteSourceControlCleanup", "Could not complete Source Control cleanup.  {FailureReason}"), FText::FromString(AccumulatedErrorsStr));
	}

	return bCleanupSuccess;
}

bool FGatherTextSCC::IsReady(FText& OutError)
{
	if( !ISourceControlModule::Get().IsEnabled() )
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "SourceControlNotEnabled", "Source control is not enabled.");
		return false;
	}

	if( !ISourceControlModule::Get().GetProvider().IsAvailable() )
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "SourceControlNotAvailable", "Source control server is currently not available.");
		return false;
	}
	return true;
}

bool FGatherTextSCC::RevertFile( const FString& InFile, FText& OutError )
{
	if( InFile.IsEmpty() || InFile.StartsWith(TEXT("\\\\")) )
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "CouldNotRevertFile", "Could not revert file.");
		return false;
	}

	FText SCCError;
	if( !IsReady( SCCError ) )
	{
		OutError = SCCError;
		return false;
	}

	FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(InFile);
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState( AbsoluteFilename, EStateCacheUsage::ForceUpdate );

	bool bSuccessfullyReverted = false;

	TArray<FString> FilesToRevert;
	FilesToRevert.Add(AbsoluteFilename);

	if( SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded() )
	{
		bSuccessfullyReverted = true;
	}
	else
	{
		bSuccessfullyReverted = (SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), FilesToRevert) == ECommandResult::Succeeded);
	}
	
	if(!bSuccessfullyReverted)
	{
		OutError = NSLOCTEXT("GatherTextCmdlet", "CouldNotRevertFile", "Could not revert file.");
	}
	else
	{
		CheckedOutFiles.Remove( AbsoluteFilename );
	}
	return bSuccessfullyReverted;
}

