// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "UnrealString.h"
#include "LocTextHelper.h"
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


class FLocFileSCCNotifies : public ILocFileNotifies
{
public:
	FLocFileSCCNotifies(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo)
		: SourceControlInfo(InSourceControlInfo)
	{
	}

	/** Virtual destructor */
	virtual ~FLocFileSCCNotifies() {}

	//~ ILocFileNotifies interface
	virtual void PreFileRead(const FString& InFilename) override {}
	virtual void PostFileRead(const FString& InFilename) override {}
	virtual void PreFileWrite(const FString& InFilename) override;
	virtual void PostFileWrite(const FString& InFilename) override;

private:
	TSharedPtr<FGatherTextSCC> SourceControlInfo;
};


struct FLocalizedAssetSCCUtil
{
	static bool SaveAssetWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UObject* InAsset);
	static bool SaveAssetWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UObject* InAsset, const FString& InFilename);

	static bool SavePackageWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UPackage* InPackage);
	static bool SavePackageWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UPackage* InPackage, const FString& InFilename);

	static bool DeleteAssetWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UObject* InAsset);

	static bool DeletePackageWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, UPackage* InPackage);

	typedef TFunctionRef<bool(const FString&)> FSaveFileCallback;
	static bool SaveFileWithSCC(const TSharedPtr<FGatherTextSCC>& InSourceControlInfo, const FString& InFilename, const FSaveFileCallback& InSaveFileCallback);
};


class IAssetRegistry;
struct FLocalizedAssetUtil
{
	static bool GetAssetsByPathAndClass(IAssetRegistry& InAssetRegistry, const FName InPackagePath, const FName InClassName, const bool bIncludeLocalizedAssets, TArray<FAssetData>& OutAssets);
	static bool GetAssetsByPathAndClass(IAssetRegistry& InAssetRegistry, const TArray<FName>& InPackagePaths, const FName InClassName, const bool bIncludeLocalizedAssets, TArray<FAssetData>& OutAssets);
};


/** Performs fuzzy path matching against a set of include and exclude paths */
class FFuzzyPathMatcher
{
public:
	enum EPathMatch
	{
		Included,
		Excluded,
		NoMatch,
	};

public:
	FFuzzyPathMatcher(const TArray<FString>& InIncludePathFilters, const TArray<FString>& InExcludePathFilters);

	EPathMatch TestPath(const FString& InPathToTest) const;

private:
	enum EPathType : uint8
	{
		Include,
		Exclude,
	};

	struct FFuzzyPath
	{
		FFuzzyPath(FString InPathFilter, const EPathType InPathType)
			: PathFilter(MoveTemp(InPathFilter))
			, PathType(InPathType)
		{
		}

		FString PathFilter;
		EPathType PathType;
	};

	TArray<FFuzzyPath> FuzzyPaths;
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
	virtual void Initialize( const TSharedPtr< FLocTextHelper >& InGatherManifestHelper, const TSharedPtr< FGatherTextSCC >& InSourceControlInfo );

	// Wrappers for extracting config values
	bool GetBoolFromConfig( const TCHAR* Section, const TCHAR* Key, bool& OutValue, const FString& Filename );
	bool GetStringFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename );
	bool GetPathFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename );
	int32 GetStringArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename );
	int32 GetPathArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename );

protected:
	TSharedPtr< FLocTextHelper > GatherManifestHelper;

	TSharedPtr< FGatherTextSCC > SourceControlInfo;

private:

	virtual void CreateCustomEngine(const FString& Params) override ; //Disallow other text commandlets to make their own engine.
	
};
