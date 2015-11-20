// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "DataTableUtils.h"

DEFINE_LOG_CATEGORY(LogDataTable);

FString DataTableUtils::AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData)
{
	FStringOutputDevice ImportError;
	if(InProp && IsSupportedTableProperty(InProp))
	{
		FString StringToImport = InString;

		const UByteProperty* EnumProp = Cast<const UByteProperty>(InProp);
		if(EnumProp && EnumProp->Enum)
		{
			// Enum properties may use the friendly name in their import data, however the UPropertyByte::ImportText function will only accept the internal enum entry name
			// Detect if we're using a friendly name for an entry, and if so, try and map it to the correct internal name before performing the import
			const int32 EnumIndex = EnumProp->Enum->FindEnumIndex(*InString);
			if(EnumIndex == INDEX_NONE)
			{
				// Couldn't find a match for the name we were given, try and find a match using the friendly names
				for(int32 EnumEntryIndex = 0; EnumEntryIndex < EnumProp->Enum->NumEnums(); ++EnumEntryIndex)
				{
					const FText FriendlyEnumEntryName = EnumProp->Enum->GetEnumText(EnumEntryIndex);
					if(FriendlyEnumEntryName.ToString() == InString)
					{
						// Get the corresponding internal name and warn the user that we're using this fallback
						StringToImport = EnumProp->Enum->GetEnumName(EnumEntryIndex);
						UE_LOG(LogDataTable, Warning, TEXT("Could not a find matching enum entry for '%s', but did find a matching display name. Will import using the enum entry corresponding to that display name ('%s')"), *InString, *StringToImport);
						break;
					}
				}
			}
		}
		
		InProp->ImportText(*StringToImport, InProp->ContainerPtrToValuePtr<uint8>(InData), PPF_None, nullptr, &ImportError);
	}

	FString Error = ImportError;
	return Error;
}

FString DataTableUtils::GetPropertyValueAsString(const UProperty* InProp, uint8* InData)
{
	FString Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		InProp->ExportText_InContainer(0, Result, InData, InData, nullptr, PPF_None);
	}

	return Result;
}

FText DataTableUtils::GetPropertyValueAsText(const UProperty* InProp, uint8* InData)
{
	FText Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		FString ExportedString;
		InProp->ExportText_InContainer(0, ExportedString, InData, InData, nullptr, PPF_PropertyWindow);
		Result = FText::FromString(ExportedString);
	}

	return Result;
}

TArray<FName> DataTableUtils::GetStructPropertyNames(UStruct* InStruct)
{
	TArray<FName> PropNames;
	for (TFieldIterator<UProperty> It(InStruct); It; ++It)
	{
		PropNames.Add(It->GetFName());
	}
	return PropNames;
}

FName DataTableUtils::MakeValidName(const FString& InString)
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

bool DataTableUtils::IsSupportedTableProperty(const UProperty* InProp)
{
	return(	InProp->IsA(UIntProperty::StaticClass()) || 
			InProp->IsA(UFloatProperty::StaticClass()) ||
			InProp->IsA(UNameProperty::StaticClass()) ||
			InProp->IsA(UStrProperty::StaticClass()) ||
			InProp->IsA(UBoolProperty::StaticClass()) ||
			InProp->IsA(UObjectPropertyBase::StaticClass()) ||
			InProp->IsA(UStructProperty::StaticClass()) ||
			InProp->IsA(UByteProperty::StaticClass()) ||
			InProp->IsA(UTextProperty::StaticClass()) ||
			InProp->IsA(UArrayProperty::StaticClass())
			);
}

FString DataTableUtils::GetPropertyDisplayName(const UProperty* Prop, const FString& DefaultName)
{
#if WITH_EDITOR
	static const FName DisplayNameKey(TEXT("DisplayName"));
	return (Prop && Prop->HasMetaData(DisplayNameKey)) ? Prop->GetMetaData(DisplayNameKey) : DefaultName;
#else  // WITH_EDITOR
	return DefaultName;
#endif // WITH_EDITOR
}
