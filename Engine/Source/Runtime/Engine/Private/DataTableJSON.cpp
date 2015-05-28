// Copyright	 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "DataTableJSON.h"
#include "Json.h"
#include "Engine/DataTable.h"
#include "Engine/UserDefinedStruct.h"
#include "DataTableUtils.h"

#if WITH_EDITOR

namespace
{
	const TCHAR* JSONTypeToString(const EJson InType)
	{
		switch(InType)
		{
		case EJson::None:
			return TEXT("None");
		case EJson::Null:
			return TEXT("Null");
		case EJson::String:
			return TEXT("String");
		case EJson::Number:
			return TEXT("Number");
		case EJson::Boolean:
			return TEXT("Boolean");
		case EJson::Array:
			return TEXT("Array");
		case EJson::Object:
			return TEXT("Object");
		default:
			return TEXT("Unknown");
		}
	}
}

FDataTableExporterJSON::FDataTableExporterJSON(const UDataTable& InDataTable, FString& OutExportText)
	: DataTable(&InDataTable)
	, JsonWriter(TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&OutExportText))
	, bJsonWriterNeedsClose(true)
{
}

FDataTableExporterJSON::FDataTableExporterJSON(const UDataTable& InDataTable, TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > InJsonWriter)
	: DataTable(&InDataTable)
	, JsonWriter(InJsonWriter)
	, bJsonWriterNeedsClose(false)
{
}

FDataTableExporterJSON::~FDataTableExporterJSON()
{
	if (bJsonWriterNeedsClose)
	{
		JsonWriter->Close();
	}
}

bool FDataTableExporterJSON::WriteTable()
{
	if (!DataTable->RowStruct)
	{
		return false;
	}

	JsonWriter->WriteArrayStart();

	// Iterate over rows
	for (auto RowIt = DataTable->RowMap.CreateConstIterator(); RowIt; ++RowIt)
	{
		JsonWriter->WriteObjectStart();
		{
			// RowName
			const FName RowName = RowIt.Key();
			JsonWriter->WriteValue(TEXT("Name"), RowName.ToString());

			// Now the values
			uint8* RowData = RowIt.Value();
			WriteRow(RowData);
		}
		JsonWriter->WriteObjectEnd();
	}

	JsonWriter->WriteArrayEnd();

	return true;
}

bool FDataTableExporterJSON::WriteRow(const void* InRowData)
{
	if (!DataTable->RowStruct)
	{
		return false;
	}

	for (TFieldIterator<UProperty> It(DataTable->RowStruct); It; ++It)
	{
		UProperty* BaseProp = *It;
		check(BaseProp);

		const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InRowData, 0);
		WriteStructEntry(InRowData, BaseProp, Data);
	}

	return true;
}

bool FDataTableExporterJSON::WriteStructEntry(const void* InRowData, const UProperty* InProperty, const void* InPropertyData)
{
	if (const UNumericProperty *NumProp = Cast<const UNumericProperty>(InProperty))
	{
		if (NumProp->IsInteger())
		{
			const int64 PropertyValue = NumProp->GetSignedIntPropertyValue(InPropertyData);
			JsonWriter->WriteValue(InProperty->GetName(), PropertyValue);
		}
		else
		{
			const double PropertyValue = NumProp->GetFloatingPointPropertyValue(InPropertyData);
			JsonWriter->WriteValue(InProperty->GetName(), PropertyValue);
		}
	}
	else if (const UBoolProperty* BoolProp = Cast<const UBoolProperty>(InProperty))
	{
		const bool PropertyValue = BoolProp->GetPropertyValue(InPropertyData);
		JsonWriter->WriteValue(InProperty->GetName(), PropertyValue);
	}
	else if (const UArrayProperty* ArrayProp = Cast<const UArrayProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(InProperty->GetName());

		FScriptArrayHelper ArrayHelper(ArrayProp, InPropertyData);
		for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < ArrayHelper.Num(); ++ArrayEntryIndex)
		{
			const void* ArrayEntryData = ArrayHelper.GetRawPtr(ArrayEntryIndex);
			WriteArrayEntry(ArrayProp->Inner, ArrayEntryData);
		}

		JsonWriter->WriteArrayEnd();
	}
	else
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData);
		JsonWriter->WriteValue(InProperty->GetName(), PropertyValue);
	}

	return true;
}

bool FDataTableExporterJSON::WriteArrayEntry(const UProperty* InProperty, const void* InPropertyData)
{
	if (const UNumericProperty *NumProp = Cast<const UNumericProperty>(InProperty))
	{
		if (NumProp->IsInteger())
		{
			const int64 PropertyValue = NumProp->GetSignedIntPropertyValue(InPropertyData);
			JsonWriter->WriteValue(PropertyValue);
		}
		else
		{
			const double PropertyValue = NumProp->GetFloatingPointPropertyValue(InPropertyData);
			JsonWriter->WriteValue(PropertyValue);
		}
	}
	else if (const UBoolProperty* BoolProp = Cast<const UBoolProperty>(InProperty))
	{
		const bool PropertyValue = BoolProp->GetPropertyValue(InPropertyData);
		JsonWriter->WriteValue(PropertyValue);
	}
	else if (const UArrayProperty* ArrayProp = Cast<const UArrayProperty>(InProperty))
	{
		// Cannot nest arrays
		return false;
	}
	else
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InPropertyData);
		JsonWriter->WriteValue(PropertyValue);
	}

	return true;
}


FDataTableImporterJSON::FDataTableImporterJSON(UDataTable& InDataTable, const FString& InJSONData, TArray<FString>& OutProblems)
	: DataTable(&InDataTable)
	, JSONData(InJSONData)
	, ImportProblems(OutProblems)
{
}

FDataTableImporterJSON::~FDataTableImporterJSON()
{
}

bool FDataTableImporterJSON::ReadTable()
{
	if (JSONData.IsEmpty())
	{
		ImportProblems.Add(TEXT("Input data is empty."));
		return false;
	}

	// Check we have a RowStruct specified
	if (!DataTable->RowStruct)
	{
		ImportProblems.Add(TEXT("No RowStruct specified."));
		return false;
	}

	TArray< TSharedPtr<FJsonValue> > ParsedTableRows;
	{
		const TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(JSONData);
		if (!FJsonSerializer::Deserialize(JsonReader, ParsedTableRows) || ParsedTableRows.Num() == 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Failed to parse the JSON data. Error: %s"), *JsonReader->GetErrorMessage()));
			return false;
		}
	}

	// Empty existing data
	DataTable->EmptyTable();

	// Iterate over rows
	for (int32 RowIdx = 0; RowIdx < ParsedTableRows.Num(); ++RowIdx)
	{
		const TSharedPtr<FJsonValue>& ParsedTableRowValue = ParsedTableRows[RowIdx];
		TSharedPtr<FJsonObject> ParsedTableRowObject = ParsedTableRowValue->AsObject();
		if (!ParsedTableRowObject.IsValid())
		{
			ImportProblems.Add(FString::Printf(TEXT("Row '%d' is not a valid JSON object."), RowIdx));
			continue;
		}

		ReadRow(ParsedTableRowObject.ToSharedRef(), RowIdx);
	}

	DataTable->Modify(true);

	return true;
}

bool FDataTableImporterJSON::ReadRow(const TSharedRef<FJsonObject>& ParsedTableRowObject, const int32 RowIdx)
{
	// Get row name
	FName RowName = DataTableUtils::MakeValidName(ParsedTableRowObject->GetStringField(TEXT("Name")));

	// Check its not 'none'
	if (RowName.IsNone())
	{
		ImportProblems.Add(FString::Printf(TEXT("Row '%d' missing a name."), RowIdx));
		return false;
	}

	// Check its not a duplicate
	if (DataTable->RowMap.Find(RowName) != nullptr)
	{
		ImportProblems.Add(FString::Printf(TEXT("Duplicate row name '%s'."), *RowName.ToString()));
		return false;
	}

	// Allocate data to store information, using UScriptStruct to know its size
	uint8* RowData = (uint8*)FMemory::Malloc(DataTable->RowStruct->PropertiesSize);
	DataTable->RowStruct->InitializeStruct(RowData);
	// And be sure to call DestroyScriptStruct later

	if (auto UDStruct = Cast<const UUserDefinedStruct>(DataTable->RowStruct))
	{
		UDStruct->InitializeDefaultValue(RowData);
	}

	// Add to row map
	DataTable->RowMap.Add(RowName, RowData);

	// Now read in each property
	for (TFieldIterator<UProperty> It(DataTable->RowStruct); It; ++It)
	{
		UProperty* BaseProp = *It;
		check(BaseProp);

		const FString PropertyName = BaseProp->GetName();
		TSharedPtr<FJsonValue> ParsedPropertyValue = ParsedTableRowObject->TryGetField(PropertyName);
		if (!ParsedPropertyValue.IsValid())
		{
			ImportProblems.Add(FString::Printf(TEXT("Row '%s' is missing an entry for '%s'."), *RowName.ToString(), *PropertyName));
			continue;
		}

		void* Data = BaseProp->ContainerPtrToValuePtr<void>(RowData, 0);
		ReadStructEntry(ParsedPropertyValue.ToSharedRef(), RowName, RowData, BaseProp, Data);
	}

	return true;
}

bool FDataTableImporterJSON::ReadStructEntry(const TSharedRef<FJsonValue>& ParsedPropertyValue, const FName InRowName, const void* InRowData, UProperty* InProperty, void* InPropertyData)
{
	const FString ColumnName = DataTableUtils::GetPropertyDisplayName(InProperty, InProperty->GetName());
	const TCHAR* const ParsedPropertyType = JSONTypeToString(ParsedPropertyValue->Type);

	if (UNumericProperty *NumProp = Cast<UNumericProperty>(InProperty))
	{
		if (NumProp->IsInteger())
		{
			int64 PropertyValue = 0;
			if (!ParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), *ColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
		else
		{
			double PropertyValue = 0.0;
			if (!ParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Double, got %s."), *ColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetFloatingPointPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (UBoolProperty* BoolProp = Cast<UBoolProperty>(InProperty))
	{
		bool PropertyValue = false;
		if (!ParsedPropertyValue->TryGetBool(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Boolean, got %s."), *ColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		BoolProp->SetPropertyValue(InPropertyData, PropertyValue);
	}
	else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(InProperty))
	{
		const TArray< TSharedPtr<FJsonValue> >* PropertyValuesPtr;
		if (!ParsedPropertyValue->TryGetArray(PropertyValuesPtr))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Array, got %s."), *ColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		FScriptArrayHelper ArrayHelper(ArrayProp, InPropertyData);
		for (const TSharedPtr<FJsonValue>& PropertyValueEntry : *PropertyValuesPtr)
		{
			const int32 NewEntryIndex = ArrayHelper.AddValue();
			void* ArrayEntryData = ArrayHelper.GetRawPtr(NewEntryIndex);
			ReadArrayEntry(PropertyValueEntry.ToSharedRef(), InRowName, ColumnName, NewEntryIndex, ArrayProp->Inner, ArrayEntryData);
		}
	}
	else
	{
		FString PropertyValue;
		if (!ParsedPropertyValue->TryGetString(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected String, got %s."), *ColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		const FString Error = DataTableUtils::AssignStringToProperty(PropertyValue, InProperty, (uint8*)InRowData);
		if(Error.Len() > 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to property '%s' on row '%s' : %s"), *PropertyValue, *ColumnName, *InRowName.ToString(), *Error));
			return false;
		}
	}

	return true;
}

bool FDataTableImporterJSON::ReadArrayEntry(const TSharedRef<FJsonValue>& ParsedPropertyValue, const FName InRowName, const FString& InColumnName, const int32 InArrayEntryIndex, UProperty* InProperty, void* InPropertyData)
{
	const TCHAR* const ParsedPropertyType = JSONTypeToString(ParsedPropertyValue->Type);

	if (UNumericProperty *NumProp = Cast<UNumericProperty>(InProperty))
	{
		if (NumProp->IsInteger())
		{
			int64 PropertyValue = 0;
			if (!ParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
		else
		{
			double PropertyValue = 0.0;
			if (!ParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Double, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetFloatingPointPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (UBoolProperty* BoolProp = Cast<UBoolProperty>(InProperty))
	{
		bool PropertyValue = false;
		if (!ParsedPropertyValue->TryGetBool(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Boolean, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		BoolProp->SetPropertyValue(InPropertyData, PropertyValue);
	}
	else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(InProperty))
	{
		// Cannot nest arrays
		return false;
	}
	else
	{
		FString PropertyValue;
		if (!ParsedPropertyValue->TryGetString(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected String, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		const FString Error = DataTableUtils::AssignStringToProperty(PropertyValue, InProperty, (uint8*)InPropertyData);
		if(Error.Len() > 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to entry %d on property '%s' on row '%s' : %s"), InArrayEntryIndex, *PropertyValue, *InColumnName, *InRowName.ToString(), *Error));
			return false;
		}
	}

	return true;
}

#endif // WITH_EDITOR
