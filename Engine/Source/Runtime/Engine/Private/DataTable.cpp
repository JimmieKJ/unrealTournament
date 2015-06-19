// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/DataTable.h"
#include "DataTableUtils.h"
#include "DataTableCSV.h"
#include "DataTableJSON.h"

DEFINE_LOG_CATEGORY(LogDataTable);

ENGINE_API const FString FDataTableRowHandle::Unknown(TEXT("UNKNOWN"));
ENGINE_API const FString FDataTableCategoryHandle::Unknown(TEXT("UNKNOWN"));

UDataTable::UDataTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDataTable::LoadStructData(FArchive& Ar)
{
	UScriptStruct* LoadUsingStruct = RowStruct;
	if (LoadUsingStruct == NULL)
	{
		UE_LOG(LogDataTable, Error, TEXT("Missing RowStruct while loading DataTable '%s'!"), *GetPathName());
		LoadUsingStruct = FTableRowBase::StaticStruct();
	}

	int32 NumRows;
	Ar << NumRows;

	for (int32 RowIdx = 0; RowIdx < NumRows; RowIdx++)
	{
		// Load row name
		FName RowName;
		Ar << RowName;

		// Load row data
		uint8* RowData = (uint8*)FMemory::Malloc(LoadUsingStruct->PropertiesSize);
		LoadUsingStruct->InitializeStruct(RowData);
		// And be sure to call DestroyScriptStruct later
		LoadUsingStruct->SerializeTaggedProperties(Ar, RowData, LoadUsingStruct, NULL);

		// Add to map
		RowMap.Add(RowName, RowData);
	}
}

void UDataTable::SaveStructData(FArchive& Ar)
{
	// Don't even try to save rows if no RowStruct
	if (RowStruct != NULL)
	{
		int32 NumRows = RowMap.Num();
		Ar << NumRows;

		// Now iterate over rows in the map
		for (auto RowIt = RowMap.CreateIterator(); RowIt; ++RowIt)
		{
			// Save out name
			FName RowName = RowIt.Key();
			Ar << RowName;

			// Save out data
			uint8* RowData = RowIt.Value();
			RowStruct->SerializeTaggedProperties(Ar, RowData, RowStruct, NULL);
		}
	}
}

void UDataTable::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar); // When loading, this should load our RowStruct!	

	if (RowStruct && RowStruct->HasAnyFlags(RF_NeedLoad))
	{
		auto RowStructLinker = RowStruct->GetLinker();
		if (RowStructLinker)
		{
			RowStructLinker->Preload(RowStruct);
		}
	}

	if(Ar.IsLoading())
	{
		EmptyTable();
		LoadStructData(Ar);
	}
	else if(Ar.IsSaving())
	{
		SaveStructData(Ar);
	}
}

void UDataTable::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UDataTable* This = CastChecked<UDataTable>(InThis);

	// Need to emit references for referenced rows
	if(This->RowStruct != NULL)
	{
		// Now iterate over rows in the map
		for ( auto RowIt = This->RowMap.CreateIterator(); RowIt; ++RowIt )
		{
			uint8* RowData = RowIt.Value();

			if (RowData)
			{
				// Serialize all of the properties to make sure they get in the collector
				FSimpleObjectReferenceCollectorArchive ObjectReferenceCollector( This, Collector );
				This->RowStruct->SerializeBin(ObjectReferenceCollector, RowData);
			}
		}
	}

#if WITH_EDITOR
	Collector.AddReferencedObjects(This->TemporarilyReferencedObjects);
#endif //WITH_EDITOR

	Super::AddReferencedObjects( This, Collector );
}

void UDataTable::FinishDestroy()
{
	Super::FinishDestroy();
	if(!IsTemplate())
	{
		EmptyTable(); // Free memory when UObject goes away
	}
}

#if WITH_EDITORONLY_DATA
void UDataTable::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add( FAssetRegistryTag(SourceFileTagName(), ImportPath, FAssetRegistryTag::TT_Hidden) );

	Super::GetAssetRegistryTags(OutTags);
}
#endif

void UDataTable::EmptyTable()
{
	UScriptStruct* LoadUsingStruct = RowStruct;
	if (LoadUsingStruct == NULL)
	{
		UE_LOG(LogDataTable, Error, TEXT("Missing RowStruct while emptying DataTable '%s'!"), *GetPathName());
		LoadUsingStruct = FTableRowBase::StaticStruct();
	}

	// Iterate over all rows in table and free mem
	for (auto RowIt = RowMap.CreateIterator(); RowIt; ++RowIt)
	{
		uint8* RowData = RowIt.Value();
		LoadUsingStruct->DestroyStruct(RowData);
		FMemory::Free(RowData);
	}

	// Finally empty the map
	RowMap.Empty();
}

/** Returns the column property where PropertyName matches the name of the column property. Returns NULL if no match is found or the match is not a supported table property */
UProperty* UDataTable::FindTableProperty(const FName& PropertyName) const
{
	UProperty* Property = NULL;
	for (TFieldIterator<UProperty> It(RowStruct); It; ++It)
	{
		Property = *It;
		check(Property != NULL);
		if (PropertyName == Property->GetFName())
		{
			break;
		}
	}
	if (!DataTableUtils::IsSupportedTableProperty(Property))
	{
		Property = NULL;
	}

	return Property;
}

#if WITH_EDITOR

void UDataTable::CleanBeforeStructChange()
{
	RowsSerializedWithTags.Reset();
	TemporarilyReferencedObjects.Empty();
	{
		class FRawStructWriter : public FObjectWriter
		{
			TSet<UObject*>& TemporarilyReferencedObjects;
		public: 
			FRawStructWriter(TArray<uint8>& InBytes, TSet<UObject*>& InTemporarilyReferencedObjects) 
				: FObjectWriter(InBytes), TemporarilyReferencedObjects(InTemporarilyReferencedObjects) {}
			virtual FArchive& operator<<(class UObject*& Res) override
			{
				FObjectWriter::operator<<(Res);
				TemporarilyReferencedObjects.Add(Res);
				return *this;
			}
		};

		FRawStructWriter MemoryWriter(RowsSerializedWithTags, TemporarilyReferencedObjects);
		SaveStructData(MemoryWriter);
	}
	EmptyTable();
	Modify();
}

void UDataTable::RestoreAfterStructChange()
{
	EmptyTable();
	{
		class FRawStructReader : public FObjectReader
		{
		public:
			FRawStructReader(TArray<uint8>& InBytes) : FObjectReader(InBytes) {}
			virtual FArchive& operator<<(class UObject*& Res) override
			{
				UObject* Object = NULL;
				FObjectReader::operator<<(Object);
				FWeakObjectPtr WeakObjectPtr = Object;
				Res = WeakObjectPtr.Get();
				return *this;
			}
		};

		FRawStructReader MemoryReader(RowsSerializedWithTags);
		LoadStructData(MemoryReader);
	}
	TemporarilyReferencedObjects.Empty();
	RowsSerializedWithTags.Empty();
}

FString UDataTable::GetTableAsString() const
{
	FString Result;

	if(RowStruct != NULL)
	{
		Result += FString::Printf(TEXT("Using RowStruct: %s\n\n"), *RowStruct->GetPathName());

		// First build array of properties
		TArray<UProperty*> StructProps;
		for (TFieldIterator<UProperty> It(RowStruct); It; ++It)
		{
			UProperty* Prop = *It;
			check(Prop != NULL);
			StructProps.Add(Prop);
		}

		// First row, column titles, taken from properties
		Result += TEXT("---");
		for(int32 PropIdx=0; PropIdx<StructProps.Num(); PropIdx++)
		{
			Result += TEXT(",");
			Result += StructProps[PropIdx]->GetName();
		}
		Result += TEXT("\n");

		// Now iterate over rows
		for ( auto RowIt = RowMap.CreateConstIterator(); RowIt; ++RowIt )
		{
			FName RowName = RowIt.Key();
			Result += RowName.ToString();

			uint8* RowData = RowIt.Value();
			for(int32 PropIdx=0; PropIdx<StructProps.Num(); PropIdx++)
			{
				Result += TEXT(",");
				Result += DataTableUtils::GetPropertyValueAsString(StructProps[PropIdx], RowData);
			}
			Result += TEXT("\n");			
		}
	}
	else
	{
		Result += FString(TEXT("Missing RowStruct!\n"));
	}
	return Result;
}

FString UDataTable::GetTableAsCSV() const
{
	FString Result;
	if (!FDataTableExporterCSV(*this, Result).WriteTable())
	{
		Result = TEXT("Missing RowStruct!\n");
	}
	return Result;
}

FString UDataTable::GetTableAsJSON() const
{
	FString Result;
	if (!FDataTableExporterJSON(*this, Result).WriteTable())
	{
		Result = TEXT("Missing RowStruct!\n");
	}
	return Result;
}

bool UDataTable::WriteRowAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter, const void* RowData) const
{
	return FDataTableExporterJSON(*this, JsonWriter).WriteRow(RowData);
}

bool UDataTable::WriteTableAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter) const
{
	return FDataTableExporterJSON(*this, JsonWriter).WriteTable();
}

/** Get array of UProperties that corresponds to columns in the table */
TArray<UProperty*> UDataTable::GetTablePropertyArray(const TArray<const TCHAR*>& Cells, UStruct* InRowStruct, TArray<FString>& OutProblems)
{
	TArray<UProperty*> ColumnProps;

	// Get list of all expected properties from the struct
	TArray<FName> ExpectedPropNames = DataTableUtils::GetStructPropertyNames(InRowStruct);

	// Need at least 2 columns, first column is skipped, will contain row names
	if(Cells.Num() > 1)
	{
		ColumnProps.AddZeroed( Cells.Num() );

		// first element always NULL - as first column is row names

		for (int32 ColIdx = 1; ColIdx < Cells.Num(); ++ColIdx)
		{
			const TCHAR* ColumnValue = Cells[ColIdx];

			FName PropName = DataTableUtils::MakeValidName(ColumnValue);
			if(PropName == NAME_None)
			{
				OutProblems.Add(FString::Printf(TEXT("Missing name for column %d."), ColIdx));
			}
			else
			{
				UProperty* ColumnProp = FindField<UProperty>(InRowStruct, PropName);

				for (TFieldIterator<UProperty> It(InRowStruct); It && !ColumnProp; ++It)
				{
					const auto DisplayName = DataTableUtils::GetPropertyDisplayName(*It, FString());
					ColumnProp = (!DisplayName.IsEmpty() && DisplayName == ColumnValue) ? *It : NULL;
				}

				// Didn't find a property with this name, problem..
				if(ColumnProp == NULL)
				{
					OutProblems.Add(FString::Printf(TEXT("Cannot find Property for column '%s' in struct '%s'."), *PropName.ToString(), *InRowStruct->GetName()));
				}
				// Found one!
				else
				{
					// Check we don't have this property already
					if(ColumnProps.Contains(ColumnProp))
					{
						OutProblems.Add(FString::Printf(TEXT("Duplicate column '%s'."), *ColumnProp->GetName()));
					}
					// Check we support this property type
					else if( !DataTableUtils::IsSupportedTableProperty(ColumnProp) )
					{
						OutProblems.Add(FString::Printf(TEXT("Unsupported Property type for struct member '%s'."), *ColumnProp->GetName()));
					}
					// Looks good, add to array
					else
					{
						ColumnProps[ColIdx] = ColumnProp;
					}

					// Track that we found this one
					ExpectedPropNames.Remove(ColumnProp->GetFName());
				}
			}
		}
	}

	// Generate warning for any properties in struct we are not filling in
	for(int32 PropIdx=0; PropIdx < ExpectedPropNames.Num(); PropIdx++)
	{
		const UProperty* const ColumnProp = FindField<UProperty>(InRowStruct, ExpectedPropNames[PropIdx]);
		const FString DisplayName = DataTableUtils::GetPropertyDisplayName(ColumnProp, ExpectedPropNames[PropIdx].ToString());
		OutProblems.Add(FString::Printf(TEXT("Expected column '%s' not found in input."), *DisplayName));
	}

	return ColumnProps;
}

TArray<FString> UDataTable::CreateTableFromCSVString(const FString& InString)
{
	// Array used to store problems about table creation
	TArray<FString> OutProblems;

	FDataTableImporterCSV(*this, InString, OutProblems).ReadTable();

	return OutProblems;
}

TArray<FString> UDataTable::CreateTableFromJSONString(const FString& InString)
{
	// Array used to store problems about table creation
	TArray<FString> OutProblems;

	FDataTableImporterJSON(*this, InString, OutProblems).ReadTable();

	return OutProblems;
}

TArray<FString> UDataTable::GetColumnTitles() const
{
	TArray<FString> Result;
	Result.Add(TEXT("Name"));
	for (TFieldIterator<UProperty> It(RowStruct); It; ++It)
	{
		UProperty* Prop = *It;
		check(Prop != NULL);
		const FString DisplayName = DataTableUtils::GetPropertyDisplayName(Prop, Prop->GetName());
		Result.Add(DisplayName);
	}
	return Result;
}

TArray<FString> UDataTable::GetUniqueColumnTitles() const
{
	TArray<FString> Result;
	Result.Add(TEXT("Name"));
	for (TFieldIterator<UProperty> It(RowStruct); It; ++It)
	{
		UProperty* Prop = *It;
		check(Prop != NULL);
		const FString DisplayName = Prop->GetName();
		Result.Add(DisplayName);
	}
	return Result;
}

TArray< TArray<FString> > UDataTable::GetTableData() const
{
	 TArray< TArray<FString> > Result;

	 Result.Add(GetColumnTitles());

	 // First build array of properties
	 TArray<UProperty*> StructProps;
	 for (TFieldIterator<UProperty> It(RowStruct); It; ++It)
	 {
		 UProperty* Prop = *It;
		 check(Prop != NULL);
		 StructProps.Add(Prop);
	 }

	 // Now iterate over rows
	 for ( auto RowIt = RowMap.CreateConstIterator(); RowIt; ++RowIt )
	 {
		 TArray<FString> RowResult;
		 FName RowName = RowIt.Key();
		 RowResult.Add(RowName.ToString());

		 uint8* RowData = RowIt.Value();
		 for(int32 PropIdx=0; PropIdx<StructProps.Num(); PropIdx++)
		 {
			 RowResult.Add(DataTableUtils::GetPropertyValueAsString(StructProps[PropIdx], RowData));
		 }
		 Result.Add(RowResult);
	 }
	 return Result;

}

#endif //WITH_EDITOR

TArray<FName> UDataTable::GetRowNames() const
{
	TArray<FName> Keys;
	RowMap.GetKeys(Keys);
	return Keys;
}
