// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Json.h"
#include "CsvParser.h"
#include "Engine/DataTable.h"
#include "Engine/UserDefinedStruct.h"

DEFINE_LOG_CATEGORY(LogDataTable);

ENGINE_API const FString FDataTableRowHandle::Unknown(TEXT("UNKNOWN"));
ENGINE_API const FString FDataTableCategoryHandle::Unknown(TEXT("UNKNOWN"));

UDataTable::UDataTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** Util that removes invalid chars and then make an FName */
FName UDataTable::MakeValidName(const FString& InString)
{
	FString InvalidChars(INVALID_NAME_CHARACTERS);

	FString FixedString;
	TArray<TCHAR>& FixedCharArray = FixedString.GetCharArray();

	// Iterate over input string characters
	for(int32 CharIdx=0; CharIdx<InString.Len(); CharIdx++)
	{
		// See if this char occurs in the InvalidChars string
		FString Char = InString.Mid( CharIdx, 1 );
		if( !InvalidChars.Contains(Char) )
		{
			// Its ok, add to result
			FixedCharArray.Add(Char[0]);
		}
	}
	FixedCharArray.Add(0);

	return FName(*FixedString);
}

/** Util to see if this property is supported in a row struct. */
bool UDataTable::IsSupportedTableProperty(const UProperty* InProp)
{
	return(	InProp->IsA(UIntProperty::StaticClass()) || 
			InProp->IsA(UFloatProperty::StaticClass()) ||
			InProp->IsA(UNameProperty::StaticClass()) ||
			InProp->IsA(UStrProperty::StaticClass()) ||
			InProp->IsA(UBoolProperty::StaticClass()) ||
			InProp->IsA(UObjectPropertyBase::StaticClass()) ||
			InProp->IsA(UStructProperty::StaticClass()) ||
			InProp->IsA(UByteProperty::StaticClass()) ||
			InProp->IsA(UTextProperty::StaticClass()));
}



/** Util to assign a value (given as a string) to a struct property. */
FString UDataTable::AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData)
{
	FStringOutputDevice ImportError;
	if(InProp != NULL && IsSupportedTableProperty(InProp))
	{
		InProp->ImportText(*InString, InProp->ContainerPtrToValuePtr<uint8>(InData), PPF_None, NULL, &ImportError);
	}

	FString Error = ImportError;
	return Error;
}

/** Util to assign get a property as a string */
FString UDataTable::GetPropertyValueAsString(const UProperty* InProp, uint8* InData)
{
	FString Result(TEXT(""));

	if(InProp != NULL && IsSupportedTableProperty(InProp))
	{
		InProp->ExportText_InContainer(0, Result, InData, InData, NULL, PPF_None);
	}

	return Result;
}

/** Util to get all property names from a struct */
TArray<FName> UDataTable::GetStructPropertyNames(UStruct* InStruct)
{
	TArray<FName> PropNames;
	for (TFieldIterator<UProperty> It(InStruct); It; ++It)
	{
		PropNames.Add(It->GetFName());
	}
	return PropNames;
}


//////////////////////////////////////////////////////////////////////////

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
				This->RowStruct->SerializeBin(ObjectReferenceCollector, RowData, 0);
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
	if (!IsSupportedTableProperty(Property))
	{
		Property = NULL;
	}

	return Property;
}

#if WITH_EDITOR

struct FPropertyDisplayNameHelper
{
	static inline FString Get(const UProperty* Prop, const FString& DefaultName)
	{
		static const FName DisplayNameKey(TEXT("DisplayName"));
		return (Prop && Prop->HasMetaData(DisplayNameKey)) ? Prop->GetMetaData(DisplayNameKey) : DefaultName;
	}
};

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

FString UDataTable::GetTableAsString()
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
		for ( auto RowIt = RowMap.CreateIterator(); RowIt; ++RowIt )
		{
			FName RowName = RowIt.Key();
			Result += RowName.ToString();

			uint8* RowData = RowIt.Value();
			for(int32 PropIdx=0; PropIdx<StructProps.Num(); PropIdx++)
			{
				Result += TEXT(",");
				Result += GetPropertyValueAsString(StructProps[PropIdx], RowData);
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

FString UDataTable::GetTableAsJSON() const
{
	// use the pretty print policy since these values are usually getting dumpped for check-in to P4 (or for inspection)
	FString Result;
	TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&Result);
	if (!WriteTableAsJSON(JsonWriter))
	{
		return TEXT("Missing RowStruct!\n");
	}
	JsonWriter->Close();
	return Result;
}

bool UDataTable::WriteTableAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter) const
{
	if (RowStruct == NULL)
	{
		return false;
	}
	JsonWriter->WriteArrayStart();

	// First build array of properties
	TArray<UProperty*> StructProps;
	for (TFieldIterator<UProperty> It(RowStruct); It; ++It)
	{
		UProperty* Prop = *It;
		check(Prop != NULL);
		StructProps.Add(Prop);
	}

	// Iterate over rows
	for (auto RowIt = RowMap.CreateConstIterator(); RowIt; ++RowIt)
	{
		JsonWriter->WriteObjectStart();

		//RowName
		FName RowName = RowIt.Key();
		JsonWriter->WriteValue(TEXT("Name"), RowName.ToString());

		//Now the values
		uint8* RowData = RowIt.Value();
		for (int32 PropIdx = 0; PropIdx < StructProps.Num(); PropIdx++)
		{
			UProperty* BaseProp = StructProps[PropIdx];
			const void* Data = BaseProp->ContainerPtrToValuePtr<void>(RowData, 0);
			if (UNumericProperty *NumProp = Cast<UNumericProperty>(StructProps[PropIdx]))
			{
				if (NumProp->IsInteger())
				{
					JsonWriter->WriteValue(BaseProp->GetName(), NumProp->GetSignedIntPropertyValue(Data));
				}
				else
				{
					JsonWriter->WriteValue(BaseProp->GetName(), NumProp->GetFloatingPointPropertyValue(Data));
				}
			}
			else if (UBoolProperty* BoolProp = Cast<UBoolProperty>(StructProps[PropIdx]))
			{
				JsonWriter->WriteValue(BaseProp->GetName(), BoolProp->GetPropertyValue(Data));
			}
			else
			{
				FString PropertyValue = GetPropertyValueAsString(BaseProp, RowData);
				JsonWriter->WriteValue(BaseProp->GetName(), PropertyValue);
			}
		}
		JsonWriter->WriteObjectEnd();
	}

	JsonWriter->WriteArrayEnd();
	return true;
}

/** Get array of UProperties that corresponds to columns in the table */
TArray<UProperty*> UDataTable::GetTablePropertyArray(const TArray<const TCHAR*>& Cells, UStruct* InRowStruct, TArray<FString>& OutProblems)
{
	TArray<UProperty*> ColumnProps;

	// Get list of all expected properties from the struct
	TArray<FName> ExpectedPropNames = GetStructPropertyNames(InRowStruct);

	// Need at least 2 columns, first column is skipped, will contain row names
	if(Cells.Num() > 1)
	{
		ColumnProps.AddZeroed( Cells.Num() );

		// first element always NULL - as first column is row names

		for (int32 ColIdx = 1; ColIdx < Cells.Num(); ++ColIdx)
		{
			const TCHAR* ColumnValue = Cells[ColIdx];

			FName PropName = MakeValidName(ColumnValue);
			if(PropName == NAME_None)
			{
				OutProblems.Add(FString(TEXT("Missing name for column %d."), ColIdx));
			}
			else
			{
				UProperty* ColumnProp = FindField<UProperty>(InRowStruct, PropName);

				for (TFieldIterator<UProperty> It(InRowStruct); It && !ColumnProp; ++It)
				{
					const auto DisplayName = FPropertyDisplayNameHelper::Get(*It, FString());
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
					else if( !IsSupportedTableProperty(ColumnProp) )
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
		const FString DisplayName = FPropertyDisplayNameHelper::Get(ColumnProp, ExpectedPropNames[PropIdx].ToString());
		OutProblems.Add(FString::Printf(TEXT("Expected column '%s' not found in input."), *DisplayName));
	}

	return ColumnProps;
}

TArray<FString> UDataTable::CreateTableFromCSVString(const FString& InString)
{
	// Array used to store problems about table creation
	TArray<FString> OutProblems;

	// Check we have a RowStruct specified
	if(RowStruct == NULL)
	{
		OutProblems.Add(FString(TEXT("No RowStruct specified.")));
		return OutProblems;
	}
	if (InString.IsEmpty())
	{
		OutProblems.Add(FString(TEXT("Input data is empty.")));
		return OutProblems;
	}

	const FCsvParser Parser(InString);
	const auto& Rows = Parser.GetRows();

	// Must have at least 2 rows (column names + data)
	if(Rows.Num() <= 1)
	{
		OutProblems.Add(FString(TEXT("Too few rows.")));
		return OutProblems;
	}

	// Find property for each column
	TArray<UProperty*> ColumnProps = GetTablePropertyArray(Rows[0], RowStruct, OutProblems);

	// Empty existing data
	EmptyTable();

	// Iterate over rows
	for(int32 RowIdx=1; RowIdx<Rows.Num(); RowIdx++)
	{
		const TArray<const TCHAR*>& Cells = Rows[RowIdx];

		// Need at least 1 cells (row name)
		if(Cells.Num() < 1)
		{
			OutProblems.Add(FString::Printf(TEXT("Row '%d' has too few cells."), RowIdx));
			continue;
		}

		// Need enough columns in the properties!
		if( ColumnProps.Num() < Cells.Num() )
		{
			OutProblems.Add(FString::Printf(TEXT("Row '%d' has more cells than properties, is there a malformed string?"), RowIdx));
			continue;
		}

		// Get row name
		FName RowName = MakeValidName(Cells[0]);

		// Check its not 'none'
		if(RowName == NAME_None)
		{
			OutProblems.Add(FString::Printf(TEXT("Row '%d' missing a name."), RowIdx));
			continue;
		}

		// Check its not a duplicate
		if(RowMap.Find(RowName) != NULL)
		{
			OutProblems.Add(FString::Printf(TEXT("Duplicate row name '%s'."), *RowName.ToString()));
			continue;
		}

		// Allocate data to store information, using UScriptStruct to know its size
		uint8* RowData = (uint8*)FMemory::Malloc(RowStruct->PropertiesSize);
		RowStruct->InitializeStruct(RowData);
		// And be sure to call DestroyScriptStruct later

#if WITH_EDITOR
		if (auto UDStruct = Cast<const UUserDefinedStruct>(RowStruct))
		{
			UDStruct->InitializeDefaultValue(RowData);
		}
#endif // WITH_EDITOR

		// Add to row map
		RowMap.Add(RowName, RowData);

		// Now iterate over cells (skipping first cell, that was row name)
		for(int32 CellIdx=1; CellIdx<Cells.Num(); CellIdx++)
		{
			// Try and assign string to data using the column property
			UProperty* ColumnProp = ColumnProps[CellIdx];
			const FString CellValue = Cells[CellIdx];
			FString Error = AssignStringToProperty(CellValue, ColumnProp, RowData);

			// If we failed, output a problem string
			if(Error.Len() > 0)
			{
				FString ColumnName = (ColumnProp != NULL) 
					? FPropertyDisplayNameHelper::Get(ColumnProp, ColumnProp->GetName())
					: FString(TEXT("NONE"));
				OutProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to property '%s' on row '%s' : %s"), *CellValue, *ColumnName, *RowName.ToString(), *Error));
			}
		}

		// Problem if we didn't have enough cells on this row
		if(Cells.Num() < ColumnProps.Num())
		{
			OutProblems.Add(FString::Printf(TEXT("Too few cells on row '%s'."), *RowName.ToString()));			
		}
	}

	Modify(true);
	return OutProblems;
}


//NOT READY YET, PLACEHOLDER
TArray<FString> UDataTable::CreateTableFromJSONString(const FString& InString)
{
	// Array used to store problems about table creation
	TArray<FString> OutProblems;

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
		const FString DisplayName = FPropertyDisplayNameHelper::Get(Prop, Prop->GetName());
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
			 RowResult.Add(GetPropertyValueAsString(StructProps[PropIdx], RowData));
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