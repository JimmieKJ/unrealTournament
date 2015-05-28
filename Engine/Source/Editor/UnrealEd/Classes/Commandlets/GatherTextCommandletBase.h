// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "UnrealString.h"
#include "InternationalizationManifest.h"
#include "Commandlets/Commandlet.h"
#include "GatherTextCommandletBase.generated.h"

class FWordCountReportData
{
public:
	FWordCountReportData()
	{

	}

	/** Gets the column count. */
	int32 GetColumnCount() const;

	/** Gets the row count. */
	int32 GetRowCount() const;

	/** Checks if the report data has a header row. */
	bool HasHeader() const;

	/**
	* Retrieves heading for a column from the header row if present.
	* @param ColumnIndex - The index of the column to get the heading for.
	* @return String representing the column heading or NULL if not found.
	*/
	const FString* GetColumnHeader(int32 ColumnIndex) const;

	/**
	* Retrieves the index of the first found column with a given heading.
	* @param ColumnHeading - The column heading to search for.
	* @return index representing the first found column with the provided heading.  INDEX_NONE if heading is not found.
	*/
	int32 GetColumnIndex(const FString& ColumnHeading) const;

	/**
	* Gets a data entry.
	* @param RowIndex - The row index to pull the table entry from.
	* @param ColumnIndex - The column index to pull the table entry from.
	* @return String representing the data entry.
	*/
	const FString* GetEntry(int32 RowIndex, int32 ColumnIndex) const;

	/**
	* Sets a data entry at the specified location.  Will not add entries if row and column indexes are outside of data range.
	* @param RowIndex - The entry row index.
	* @param ColumnIndex - The entry column index.
	* @param EntryString - The string to set at specified location.
	*/
	void SetEntry(int32 RowIndex, int32 ColumnIndex, const FString& EntryString);
		
	/**
	* Sets a data entry at the specified location.  Will add a new column if the provided heading is not found.
	* @param RowIndex - The entry row index.
	* @param ColumnHeader - The column heading for the row entry.  If this string is not found in the header row, a new column will be added.
	* @param EntryString - The string to set at specified location.
	*/
	void SetEntry(int32 RowIndex, const FString& ColumnHeading, const FString& EntryString);
		
	/**
	* Populates data structure from comma separated CSV format.
	* @param InString - String with comma separated data
	* @return true if data was parsed successfully, false otherwise
	*/
	bool FromCSV( const FString& InString );

	/**
	* Writes data to a comma separated string suitable for writing to CSV file.
	* @return Comma separated string
	*/
	FString ToCSV() const;

	/**
	* Adds a row with blank entries and returns the row index.
	* @return The index of the added row.
	*/
	int32 AddRow();

	/**
	* Adds a column with blank entries and returns the row index.
	* @param ColumnHeading - Optional parameter that specifies the column heading.  Will only be used if the data has a header.
	* @return The index of the added column.
	*/
	int32 AddColumn(const FString* ColumnHeading=NULL);
	

	static const TCHAR* EntryDelimiter;
	static const TCHAR* NewLineDelimiter;
	static const FString ColHeadingDateTime;
	static const FString ColHeadingWordCount;

private:	
	TArray< TArray<FString> > Data;
};

/** Helper struct for tracking duplicate localization entries */
struct FLocConflict
{
public:

	FLocConflict( const FString& InNamespace, const FString& InKey, const TSharedPtr<FLocMetadataObject>& InKeyMetadataObj )
		: Namespace( InNamespace )
		, Key( InKey )
		, KeyMetadataObj( InKeyMetadataObj )
	{
	}

	void Add( const FLocItem& Source, const FString& SourceLocation );

	const FString Namespace;
	const FString Key;
	TSharedPtr<FLocMetadataObject> KeyMetadataObj;

	TMultiMap<FString, FLocItem> EntriesBySourceLocation;
};

typedef TMultiMap< FString, TSharedRef< FLocConflict > > TLocConflictContainer;

struct FConflictReportInfo
{
	/**
	 * Return the singleton instance of the report info.
	 * @return The singleton instance.
	 */
	static FConflictReportInfo& GetInstance();

	/**
	 * Deletes the singleton instance of the report info.
	 */
	static void DeleteInstance();

	/**
	 * Finds a conflict entry based on namespace and key.
	 * @param Namespace - The namespace of the entry.
	 * @param Key - The key/identifier of the entry.
	 * @return A pointer to the conflict entry or null if not found.
	 */
	TSharedPtr< FLocConflict > FindEntryByKey( const FString& Namespace, const FString& Key, const TSharedPtr<FLocMetadataObject> KeyMetadata );

	/**
	 * Gets the iterator that can be used to traverse all the conflict entries.
	 *
	 * @return A const iterator.
	 */
	TLocConflictContainer::TConstIterator GetEntryConstIterator() const
	{
		return EntriesByKey.CreateConstIterator();
	}
	
	/**
	 * Adds a conflict entry.
	 * @param Namespace - The namespace of the entry.
	 * @param Key - The key/identifier of the entry.
	 * @param KeyMetadata - Entry Metadata keys.
	 * @param Source - The source info for the conflict.
	 * @param SourceLocation - The source location of the conflict.
	 */
	void AddConflict(const FString& Namespace, const FString& Key, const TSharedPtr<FLocMetadataObject> KeyMetadata, const FLocItem& Source, const FString& SourceLocation );

	/**
	 * Return the report in string format.
	 *
	 * @return String representing the report.
	 */
	FString ToString();

private:
	FConflictReportInfo() {}
	FConflictReportInfo( const FConflictReportInfo& );
	FConflictReportInfo& operator=( const FConflictReportInfo& );


private:
	static TSharedPtr<FConflictReportInfo> StaticConflictInstance;
	TLocConflictContainer EntriesByKey;
};


class FManifestInfo
{
public:
 	FManifestInfo( )
	{
		Manifest = MakeShareable( new FInternationalizationManifest() );
	}

	// Given a list of manifest file paths, this will load and add the files to the dependency list. ApplyManifestDependencies must be explicitly called to remove dependency entries from the manifest.
	bool AddManifestDependencies( const TArray< FString >& InManifestFiles );

	// Given a manifest file path will load and add that file to the dependency list. ApplyManifestDependencies must be explicitly called to remove dependency entries from the manifest.
	bool AddManifestDependency( const FString& InManifestFile );

	// Return the first entry in the dependencies that matches the passed in namespace and context.
	TSharedPtr< FManifestEntry > FindDependencyEntrybyContext(  const FString& Namespace, const FContext& Context, FString& OutFileName );

	// Return the first entry in the dependencies that matches the passed in namespace and source.
	TSharedPtr< FManifestEntry > FindDependencyEntrybySource( const FString& Namespace, const FLocItem& Source, FString& OutFileName );

	// Returns the number of manifest dependencies
	int32 NumDependencies() { return ManifestDependencies.Num(); }

	// Strips the current manifest of any entries that appear in the dependencies.  This is not automatically called when dependency files are added.
	void ApplyManifestDependencies();

	// Adds an entry to the manifest if it is not in one of the manifest dependencies
	bool AddEntry( const FString& EntryDescription, const FString& Namespace, const FLocItem& Source, const FContext& Context );

	TSharedPtr< class FInternationalizationManifest > GetManifest() { return Manifest; }

private:
	TArray < FString > ManifestDependenciesFilePaths;
	TArray < TSharedPtr< class FInternationalizationManifest > >  ManifestDependencies;

	TSharedPtr< class FInternationalizationManifest > Manifest;

};



class FGatherTextSCC
{
public:
	FGatherTextSCC();

	~FGatherTextSCC();

	bool CheckOutFile(const FString& InFile, FText& OutError);
	bool CheckinFiles( const FText& InChangeDescription, FText& OutError );
	bool CleanUp( FText& OutError );
	bool RevertFile(const FString& InFile, FText& OutError);
	bool IsReady(FText& OutError);

private:
	TArray<FString> CheckedOutFiles;
};


/**
 *	UGatherTextCommandletBase: Base class for localization commandlets. Just to force certain behaviors and provide helper functionality. 
 */
class FJsonValue;
class FJsonObject;
UCLASS()
class UGatherTextCommandletBase : public UCommandlet
{
	GENERATED_UCLASS_BODY()


public:
	virtual void Initialize( const TSharedRef< FManifestInfo >& InManifestInfo, const TSharedPtr< FGatherTextSCC >& InSourceControlInfo );

	static TSharedPtr<FJsonObject> ReadJSONTextFile( const FString& InFilePath ) ;
	static bool WriteJSONToTextFile( TSharedPtr<FJsonObject> Output, const FString& Filename, TSharedPtr<FGatherTextSCC> SourceControl );

	// Used to replace strings found in localization text that could cause automated builds to fail if seen in the log output
	static FString MungeLogOutput( const FString& InString );

	// Wrappers for extracting config values
	bool GetBoolFromConfig( const TCHAR* Section, const TCHAR* Key, bool& OutValue, const FString& Filename );
	bool GetStringFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename );
	bool GetPathFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename );
	int32 GetStringArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename );
	int32 GetPathArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename );

protected:
	TSharedPtr< FManifestInfo > ManifestInfo;

	TSharedPtr< FGatherTextSCC > SourceControlInfo;

private:

	virtual void CreateCustomEngine(const FString& Params) override ; //Disallow other text commandlets to make their own engine.
	
};
