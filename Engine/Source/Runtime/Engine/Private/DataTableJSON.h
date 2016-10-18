// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataTableUtils.h"

class UDataTable;
class FJsonValue;
class FJsonObject;

// forward declare JSON writer
template <class CharType>
struct TPrettyJsonPrintPolicy;
template <class CharType, class PrintPolicy>
class TJsonWriter;

#if WITH_EDITOR

class FDataTableExporterJSON
{
public:
	FDataTableExporterJSON(const EDataTableExportFlags InDTExportFlags, FString& OutExportText);

	FDataTableExporterJSON(const EDataTableExportFlags InDTExportFlags, TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > InJsonWriter);

	~FDataTableExporterJSON();

	bool WriteTable(const UDataTable& InDataTable);

	bool WriteRow(const UScriptStruct* InRowStruct, const void* InRowData);

	bool WriteStruct(const UScriptStruct* InStruct, const void* InStructData);

private:
	bool WriteStructEntry(const void* InRowData, const UProperty* InProperty, const void* InPropertyData);

	bool WriteArrayEntry(const UProperty* InProperty, const void* InPropertyData);

	EDataTableExportFlags DTExportFlags;
	TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > JsonWriter;
	bool bJsonWriterNeedsClose;
};

class FDataTableImporterJSON
{
public:
	FDataTableImporterJSON(UDataTable& InDataTable, const FString& InJSONData, TArray<FString>& OutProblems);

	~FDataTableImporterJSON();

	bool ReadTable();

private:
	bool ReadRow(const TSharedRef<FJsonObject>& InParsedTableRowObject, const int32 InRowIdx);

	bool ReadStruct(const TSharedRef<FJsonObject>& InParsedObject, UScriptStruct* InStruct, const FName InRowName, void* InStructData);

	bool ReadStructEntry(const TSharedRef<FJsonValue>& InParsedPropertyValue, const FName InRowName, const FString& InColumnName, const void* InRowData, UProperty* InProperty, void* InPropertyData);

	bool ReadArrayEntry(const TSharedRef<FJsonValue>& InParsedPropertyValue, const FName InRowName, const FString& InColumnName, const int32 InArrayEntryIndex, UProperty* InProperty, void* InPropertyData);

	UDataTable* DataTable;
	const FString& JSONData;
	TArray<FString>& ImportProblems;
};

#endif // WITH_EDITOR
