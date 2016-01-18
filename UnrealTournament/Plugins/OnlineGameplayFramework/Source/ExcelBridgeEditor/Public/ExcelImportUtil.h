// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExcelImportUtil.generated.h"

class FExcelBridge;

/**
 This is the struct you will get in OnImportTables
*/
struct EXCELBRIDGEEDITOR_API FTableRecord
{
	FString TableName;
	TArray<FString> ColumnNames;
	TArray< TArray<FString> > Rows;

	TArray<int32> MapNamesToColumns(const TArray<FString>& Names, bool* bAllMappedSuccessfullyOut) const;
	TArray<const UProperty*> MapColumnsToProperties(const UStruct* Struct, bool* bAllMappedSuccessfullyOut) const;

	bool RowToProperties(int32 RowIdx, const TArray<const UProperty*>& PropertyMap, void* StructValueOut) const;
};

/**
 Drop this struct into your class as a UPROPERTY(EditAnywhere) and then call Setup()
 in your PostInitializeProperties.

 This struct has a details customization which presents UI for  
*/
USTRUCT()
struct EXCELBRIDGEEDITOR_API FExcelImportUtil
{
	GENERATED_USTRUCT_BODY()
public:

	// specify a new file to import from
	bool ImportFromXlsFile(const FString& FileName);

	// reimport from the last source
	bool Reimport();

	// Tries to connect to a running excel and import from it
	bool ImportFromRunningExcelInstance();

	// get the last imported file path
	FORCEINLINE const FString& GetLastFilePath() const { return LastExcelFilePath; }

public: // populate these fields in PostInitializeProperties

	// table names that must be present in excel
	TArray<FString> RequiredTables;

	// table names included in ImportedTables if they are found
	TArray<FString> OptionalTables;

	// delegate called when the user reimports data from excel
	typedef TMap<FString, TSharedRef<FTableRecord> > TableMap;
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnImportTables, const TableMap&);
	FOnImportTables OnImportTables;

	// holds the text of the last error message logged
	FString LastErrorMessage;

private:
	bool ExportInternal(FExcelBridge* Bridge);

private:

	// store the last XLS file we imported from so we can support quick reimport (relative path)
	UPROPERTY()
	FString LastExcelFilePath;
};
