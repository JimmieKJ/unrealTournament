// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ExcelBridgeEditorPrivatePCH.h"
#include "ExcelImportUtil.h"
#include "ExcelBridgeEditorModule.h"
#include "ExcelBridge.h"

TArray<int32> FTableRecord::MapNamesToColumns(const TArray<FString>& Names, bool* bAllMappedSuccessfullyOut) const
{
	TArray<int32> Out;
	Out.Reserve(Names.Num());
	bool bFoundAllNames = true;
	
	// find each name in the column names list
	for (const FString& Name : Names)
	{
		int32 Index = -1;
		for (int32 i=0;i<ColumnNames.Num();++i)
		{
			if (ColumnNames[i] == Name)
			{
				Index = i;
				break;
			}
		}
		Out.Push(Index);
	}

	// add the out param
	if (bAllMappedSuccessfullyOut != nullptr)
	{
		*bAllMappedSuccessfullyOut = bFoundAllNames;
	}
	return Out;
}

TArray<const UProperty*> FTableRecord::MapColumnsToProperties(const UStruct* Struct, bool* bAllMappedSuccessfullyOut) const
{
	TArray<const UProperty*> Result;
	Result.Reserve(ColumnNames.Num());

	bool bFoundAllColumns = true;
	for (const FString& ColumnName : ColumnNames)
	{
		// find a property by that name
		UProperty* Property = nullptr;
		for (TFieldIterator<UProperty> It(Struct); It; ++It)
		{
			UProperty* Prop = *It;
			if (Prop->GetName() == ColumnName)
			{
				Property = Prop;
				break;
			}
		}

		// if we couldn't find it, note that fact
		if (Property == nullptr)
		{
			bFoundAllColumns = false;
		}

		// push the result
		Result.Push(Property);
	}

	if (bAllMappedSuccessfullyOut != nullptr)
	{
		*bAllMappedSuccessfullyOut = bFoundAllColumns;
	}
	check(Result.Num() == ColumnNames.Num());
	return Result;
}


bool FTableRecord::RowToProperties(int32 RowIdx, const TArray<const UProperty*>& PropertyMap, void* StructValueOut) const
{
	// make sure the property map was generated from this record
	check(PropertyMap.Num() == ColumnNames.Num());

	// get the row
	if (RowIdx < 0 || RowIdx >= Rows.Num())
	{
		UE_LOG(LogExcelBridgeEditor, Error, TEXT("RowIdx %d is out of bounds on %s"), RowIdx, *TableName);
		return false;
	}
	const TArray<FString>& Row = Rows[RowIdx];
	check(Row.Num() == ColumnNames.Num());

	// map each column
	bool bMappedAll = true;
	for (int i = 0, n = ColumnNames.Num(); i < n; ++i)
	{
		// get the property
		const UProperty* Property = PropertyMap[i];
		if (Property == nullptr)
		{
			// no column mapping
			continue;
		}

		// check the cell value
		const FString& CellValue = Row[i];
		if (CellValue.IsEmpty())
		{
			// empty cells can keep their default value
			continue;
		}

		// convert the text to that property
		void* Value = Property->ContainerPtrToValuePtr<uint8>(StructValueOut);
		if (Property->ImportText(*CellValue, Value, 0, nullptr) == nullptr)
		{
			bMappedAll = false;
			UE_LOG(LogExcelBridgeEditor, Error, TEXT("Unable to import text to %s - \"%s\""), *Property->GetName(), *CellValue);
			continue;
		}
	}
	return bMappedAll;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOG_IMPORT_ERROR(...) \
	LastErrorMessage = FString::Printf(__VA_ARGS__); \
	UE_LOG(LogExcelBridgeEditor, Error, TEXT("%s"), *LastErrorMessage)
	

bool FExcelImportUtil::ImportFromXlsFile(const FString& FileName)
{
	// make a new excel bridge instance
	FExcelBridgeEditorModule& Module = FExcelBridgeEditorModule::Get();
	FExcelBridge* Bridge = Module.NewExcelBridge();
	if (Bridge == nullptr)
	{
		// error should already be logged
		return false;
	}

	// convert to absolute path
	FString FullPath = FPaths::ConvertRelativePathToFull(FileName);

	// open a workbook by filename
	if (!Bridge->OpenWorkbook(*FullPath))
	{
		LOG_IMPORT_ERROR(TEXT("Unable to open workbook at \"%s\": %s"), *FullPath, Bridge->GetError());
		Module.DeleteExcelBridge(Bridge);
		return false;
	}

	// export the tables
	if (!ExportInternal(Bridge))
	{
		// error should already be logged
		Module.DeleteExcelBridge(Bridge);
		return false;
	}

	// save the path if it's successful
	Module.DeleteExcelBridge(Bridge);
	LastExcelFilePath = FileName;
	return true;
}

bool FExcelImportUtil::Reimport()
{
	if (LastExcelFilePath.IsEmpty())
	{
		LOG_IMPORT_ERROR(TEXT("No previous import file found."));
		return false;
	}
	return ImportFromXlsFile(LastExcelFilePath);
}

bool FExcelImportUtil::ImportFromRunningExcelInstance()
{
	// make a new excel bridge instance
	FExcelBridgeEditorModule& Module = FExcelBridgeEditorModule::Get();
	FExcelBridge* Bridge = Module.NewExcelBridge();
	if (Bridge == nullptr)
	{
		// error should already be logged
		return false;
	}

	// open a workbook by filename
	if (!Bridge->OpenActiveWorkbook())
	{
		LOG_IMPORT_ERROR(TEXT("Unable to find an active Excel instance with an open workbook.\n%s"), Bridge->GetError());
		Module.DeleteExcelBridge(Bridge);
		return false;
	}

	// export the tables
	bool bSuccess = ExportInternal(Bridge);

	// release resources
	Module.DeleteExcelBridge(Bridge);
	return bSuccess;
}

static bool ReadString(FString& ValueOut, const wchar_t*& Ptr, const wchar_t* End)
{
	if (Ptr >= End)
	{
		// no strings left (should have at least been a '\0')
		return false;
	}
	const wchar_t* Start = Ptr;
	while (*Ptr != '\0')
	{
		++Ptr;
		if (Ptr >= End)
		{
			// reached data end before string termination
			return false;
		}
	}
	ValueOut.AppendChars(Start, Ptr - Start);
	++Ptr;
	return true;
}

static TSharedPtr<FTableRecord> BuildTableRecord(const FString& TableName, const FExcelBridge::Table& Table, FString& LastErrorMessage)
{
	check(Table.Width > 0);
	check(Table.Height > 0);

	// make a record
	TSharedRef<FTableRecord> Record = MakeShareable(new FTableRecord());
	Record->TableName = TableName;
	Record->ColumnNames.SetNum(Table.Width);

	// first row is column names
	const wchar_t* Start = Table.Data;
	const wchar_t* End = Start + Table.DataSize;
	for (int i = 0; i < Table.Width; ++i)
	{
		if (!ReadString(Record->ColumnNames[i], Start, End))
		{
			LOG_IMPORT_ERROR(TEXT("Ran out of data strings while reading column name %d from table %s"), i, *TableName);
			return TSharedPtr<FTableRecord>();
		}
	}

	// now read rows
	Record->Rows.SetNum(Table.Height - 1);
	for (int i = 0; i < Table.Height - 1; ++i)
	{
		TArray<FString>& Row = Record->Rows[i];
		Row.SetNum(Table.Width);
		for (int j = 0; j < Table.Width; ++j)
		{
			if (!ReadString(Row[j], Start, End))
			{
				LOG_IMPORT_ERROR(TEXT("Ran out of data strings while reading column %d from row %d of table %s"), j, i, *TableName);
				return TSharedPtr<FTableRecord>();
			}
		}
	}

	return Record;
}

bool FExcelImportUtil::ExportInternal(FExcelBridge* Bridge)
{
	check(Bridge);
	// make sure our delegate is bound (or there's no point)
	if (!OnImportTables.IsBound())
	{
		LOG_IMPORT_ERROR(TEXT("OnImportTables was not bound. Failing import."));
		return false;
	}

	// log all the tables available
#if 0
	{
		FExcelBridge::Table TableList = Bridge->GetTableList();
		check (TableList.Height == 1);
		const wchar_t* Start = TableList.Data;
		const wchar_t* End = Start + TableList.DataSize;
		for (int i=0;i<TableList.Width; ++i)
		{
			FString TableName;
			ReadString(TableName, Start, End);
			UE_LOG(LogExcelBridgeEditor, Display, TEXT("Available Table: %s"), *TableName);
		}
	}
#endif

	// build a map of imported tables
	TMap<FString, TSharedRef<FTableRecord> > Tables;

	// try to get all required tables
	for (const FString& TableName : RequiredTables)
	{
		// export the table
		FExcelBridge::Table Table = Bridge->ExportTable(*TableName);
		if (Table.Width <= 0 || Table.Height <= 0)
		{
			LOG_IMPORT_ERROR(TEXT("Failed to import required table %s"), *TableName);
			return false;
		}

		// build a record
		TSharedPtr<FTableRecord> TableRec = BuildTableRecord(TableName, Table, LastErrorMessage);
		if (!TableRec.IsValid())
		{
			LOG_IMPORT_ERROR(TEXT("Failed to parse required table %s"), *TableName);
			return false;
		}

		// add to the mapping
		Tables.Add(TableName, TableRec.ToSharedRef());
	}

	// try to get any optional tables
	for (const FString& TableName : RequiredTables)
	{
		// export the table
		FExcelBridge::Table Table = Bridge->ExportTable(*TableName);
		if (Table.Width <= 0 || Table.Height <= 0)
		{
			// optional table
			continue;
		}

		// build a record
		TSharedPtr<FTableRecord> TableRec = BuildTableRecord(TableName, Table, LastErrorMessage);
		if (!TableRec.IsValid())
		{
			LOG_IMPORT_ERROR(TEXT("Failed to parse optional table %s"), *TableName);
			return false;
		}

		// add to the mapping
		Tables.Add(TableName, TableRec.ToSharedRef());
	}

	// run the delegate
	return OnImportTables.Execute(Tables);
}
