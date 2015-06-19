// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

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
	FDataTableExporterJSON(const UDataTable& InDataTable, FString& OutExportText);

	FDataTableExporterJSON(const UDataTable& InDataTable, TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > InJsonWriter);

	~FDataTableExporterJSON();

	bool WriteTable();

	bool WriteRow(const void* InRowData);

private:
	bool WriteStructEntry(const void* InRowData, const UProperty* InProperty, const void* InPropertyData);

	bool WriteArrayEntry(const UProperty* InProperty, const void* InPropertyData);

	const UDataTable* DataTable;
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
	bool ReadRow(const TSharedRef<FJsonObject>& ParsedTableRowObject, const int32 RowIdx);

	bool ReadStructEntry(const TSharedRef<FJsonValue>& ParsedPropertyValue, const FName InRowName, const void* InRowData, UProperty* InProperty, void* InPropertyData);

	bool ReadArrayEntry(const TSharedRef<FJsonValue>& ParsedPropertyValue, const FName InRowName, const FString& InColumnName, const int32 InArrayEntryIndex, UProperty* InProperty, void* InPropertyData);

	UDataTable* DataTable;
	const FString& JSONData;
	TArray<FString>& ImportProblems;
};

#endif // WITH_EDITOR
