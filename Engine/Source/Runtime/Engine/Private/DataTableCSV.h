// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UDataTable;

#if WITH_EDITOR

class FDataTableExporterCSV
{
public:
	FDataTableExporterCSV(const UDataTable& InDataTable, FString& OutExportText);

	~FDataTableExporterCSV();

	bool WriteTable();

	bool WriteRow(const void* InRowData);

private:
	bool WriteStructEntry(const void* InRowData, UProperty* InProperty, const void* InPropertyData);

	const UDataTable* DataTable;
	FString& ExportedText;
};

class FDataTableImporterCSV
{
public:
	FDataTableImporterCSV(UDataTable& InDataTable, FString InCSVData, TArray<FString>& OutProblems);

	~FDataTableImporterCSV();

	bool ReadTable();

private:
	UDataTable* DataTable;
	FString CSVData;
	TArray<FString>& ImportProblems;
};

#endif // WITH_EDITOR
