// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataTable.generated.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogDataTable, Log, All);

// forward declare JSON writer
template <class CharType>
struct TPrettyJsonPrintPolicy;
template <class CharType, class PrintPolicy>
class TJsonWriter;

/** Base class for all table row structs to inherit from */
USTRUCT()
struct FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	FTableRowBase()
	{
	}
};

/** Imported spreadsheet table */
UCLASS(MinimalAPI)
class UDataTable : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Structure to use for each row of the table, must inherit from FTableRowBase */
	UPROPERTY()
	UScriptStruct*			RowStruct;

	/** The filename that was used to create this object. Relative to the object's package, BaseDir() or absolute */
	UPROPERTY()
	FString ImportPath;

	/** Map of name of row to row data structure. */
	TMap<FName, uint8*>		RowMap;

	// Begin UObject interface.
	virtual void FinishDestroy() override;
	virtual void Serialize( FArchive& Ar ) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End  UObject interface

	// Begin UDataTable interface

	/** Function to find the row of a table given its name. */
	template <class T>
	T* FindRow(FName RowName, const FString& ContextString, bool bWarnIfRowMissing = true) const
	{
		if(RowStruct == NULL)
		{
			UE_LOG(LogDataTable, Error, TEXT("UDataTable::FindRow : DataTable '%s' has no RowStruct specified (%s)."), *GetPathName(), *ContextString);
			return NULL;
		}

		if(!RowStruct->IsChildOf(T::StaticStruct()))
		{
			UE_CLOG(bWarnIfRowMissing, LogDataTable, Error, TEXT("UDataTable::FindRow : Incorrect type specified for DataTable '%s' (%s)."), *GetPathName(), *ContextString);
			return NULL;
		}

		if(RowName == NAME_None)
		{
			UE_CLOG(bWarnIfRowMissing, LogDataTable, Warning, TEXT("UDataTable::FindRow : NAME_None is invalid row name for DataTable '%s' (%s)."), *GetPathName(), *ContextString);
			return NULL;
		}

		uint8* const* RowDataPtr = RowMap.Find(RowName);
		if (RowDataPtr == NULL)
		{
			if (bWarnIfRowMissing)
			{
				UE_LOG(LogDataTable, Warning, TEXT("UDataTable::FindRow : Row '%s' not found in DataTable '%s' (%s)."), *RowName.ToString(), *GetPathName(), *ContextString);
			}
			return NULL;
		}

		uint8* RowData = *RowDataPtr;
		check(RowData);

		return (T*)RowData;
	}

	/** Returns the column property where PropertyName matches the name of the column property. Returns NULL if no match is found or the match is not a supported table property */
	ENGINE_API UProperty* FindTableProperty(const FName& PropertyName) const;

	uint8* FindRowUnchecked(FName RowName, bool MustExist=false) const
	{
		if(RowStruct == NULL)
		{
			//UE_CLOG(MustExist, LogDataTable, Error, TEXT("UDataTable::FindRow : DataTable '%s' has no RowStruct specified (%s)."), *GetPathName(), *ContextString);
			return NULL;
		}

		if(RowName == NAME_None)
		{
			//UE_CLOG(MustExist, LogDataTable, Warning, TEXT("UDataTable::FindRow : NAME_None is invalid row name for DataTable '%s' (%s)."), *GetPathName(), *ContextString);
			return NULL;
		}

		uint8* const* RowDataPtr = RowMap.Find(RowName);

		if(RowDataPtr == NULL)
		{
			return NULL;
		}

		uint8* RowData = *RowDataPtr;
		check(RowData);

		return RowData;
	}

	/** Empty the table info (will not clear RowStruct) */
	ENGINE_API void EmptyTable();

	ENGINE_API TArray<FName> GetRowNames() const;

#if WITH_EDITOR

private:
	//when RowStruct is being modified, row data is stored serialized with tags
	TArray<uint8> RowsSerializedWithTags;
	TSet<UObject*> TemporarilyReferencedObjects;
public:
	ENGINE_API void CleanBeforeStructChange();
	ENGINE_API void RestoreAfterStructChange();

	/** Output entire contents of table as a string */
	ENGINE_API FString GetTableAsString();

	/** Output entire contents of table as JSON */
	ENGINE_API FString GetTableAsJSON() const;

	/** Output entire contents of table as JSON */
	ENGINE_API bool WriteTableAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter) const;

	/** 
	 *	Create table from CSV style comma-separated string. 
	 *	RowStruct must be defined before calling this function. 
	 *	@return	Set of problems encountered while processing input
	 */
	ENGINE_API TArray<FString> CreateTableFromCSVString(const FString& InString);

	/** 
	*	Create table from JSON style string. 
	*	RowStruct must be defined before calling this function. 
	*	@return	Set of problems encountered while processing input
	*/
	ENGINE_API TArray<FString> CreateTableFromJSONString(const FString& InString);

	/** Get array of all the column titles */
	ENGINE_API TArray<FString> GetColumnTitles() const;

	TArray<UProperty*> GetTablePropertyArray(const TArray<const TCHAR*>& Cells, UStruct* RowStruct, TArray<FString>& OutProblems);

	/** Get array for each row in the table. The first row is the titles*/
	ENGINE_API TArray< TArray<FString> > GetTableData() const;
#endif //WITH_EDITOR

	ENGINE_API static FString AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData);

	ENGINE_API static FString GetPropertyValueAsString(const UProperty* InProp, uint8* InData);

	ENGINE_API static TArray<FName> GetStructPropertyNames(UStruct* InStruct);

	ENGINE_API static FName MakeValidName(const FString& InString);

	ENGINE_API static bool IsSupportedTableProperty(const UProperty* InProp);

	// End UDataTable interface

private:
	void SaveStructData(FArchive& Ar);
	void LoadStructData(FArchive& Ar);
};

/** Handle to a particular row in a table*/
USTRUCT(BlueprintType)
struct ENGINE_API FDataTableRowHandle
{
	GENERATED_USTRUCT_BODY()

	FDataTableRowHandle()
		: DataTable(NULL)
		, RowName(NAME_None)
	{

	}

	/** Pointer to table we want a row from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableRowHandle)
	const class UDataTable*	DataTable;

	/** Name of row in the table that we want */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableRowHandle)
	FName				RowName;

	static const FString Unknown;

	/** Get the row straight from the row handle */
	template <class T>
	T* GetRow(const FString& ContextString = Unknown) const
	{
		if(DataTable == NULL)
		{
			if (RowName != NAME_None)
			{
				UE_LOG(LogDataTable, Warning, TEXT("FDataTableRowHandle::FindRow : No DataTable for row %s (%s)."), *RowName.ToString(), *ContextString);
			}
			return NULL;
		}

		return DataTable->FindRow<T>(RowName, ContextString);
	}
};

/** Handle to a particular row in a table*/
USTRUCT()
struct ENGINE_API FDataTableCategoryHandle
{
	GENERATED_USTRUCT_BODY()

	/** Pointer to table we want a row from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableCategoryHandle)
	const class UDataTable*	DataTable;

	/** Name of column in the table that we want */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableCategoryHandle)
	FName				ColumnName;

	/** Contents of rows in the table that we want */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableCategoryHandle)
	FName				RowContents;

	static const FString Unknown;

	/** Searches DataTable for all rows that contain entries with RowContents in the column named ColumnName and returns them. */
	template <class T>
	void GetRows(TArray<T*>& OutRows, const FString& ContextString = Unknown) const
	{
		OutRows.Empty();
		if (DataTable == NULL)
		{
			if (RowContents != NAME_None)
			{
				UE_LOG(LogDataTable, Warning, TEXT("FDataTableCategoryHandle::FindRow : No DataTable for row %s (%s)."), *RowContents.ToString(), *ContextString);
			}

			return;
		}

		if (ColumnName == NAME_None)
		{
			if (RowContents != NAME_None)
			{
				UE_LOG(LogDataTable, Warning, TEXT("FDataTableCategoryHandle::FindRow : No Column selected for row %s (%s)."), *RowContents.ToString(), *ContextString);
			}

			return;
		}

		// Find the property that matches the desired column (ColumnName)
		UProperty* Property = DataTable->FindTableProperty(ColumnName);
		if (Property == NULL)
		{
			return;
		}

		// check each row to see if the value in the Property element is the one we're looking for (RowContents). If it is, add the row to OutRows
		FString RowContentsAsString = RowContents.ToString();

		for (auto RowIt = DataTable->RowMap.CreateConstIterator(); RowIt; ++RowIt)
		{
			uint8* RowData = RowIt.Value();

			FString PropertyValue(TEXT(""));

			Property->ExportText_InContainer(0, PropertyValue, RowData, RowData, NULL, PPF_None);

			if (RowContentsAsString == PropertyValue)
			{
				OutRows.Add((T*)RowData);
			}
		}

		return;
	}
};

/** Macro to call GetRow with a correct error info. Assumed to be called from within a UObject */
#define GETROW_REPORTERROR(Handle, Template) Handle.GetRow<Template>(FString::Printf(TEXT("%s.%s"), *GetPathName(), TEXT(#Handle)))
#define GETROWOBJECT_REPORTERROR(Object, Handle, Template) Handle.GetRow<Template>(FString::Printf(TEXT("%s.%s"), *Object->GetPathName(), TEXT(#Handle)))
