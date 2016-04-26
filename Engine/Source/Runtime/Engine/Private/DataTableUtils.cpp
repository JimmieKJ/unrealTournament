// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "DataTableUtils.h"

DEFINE_LOG_CATEGORY(LogDataTable);

namespace DataTableUtilsImpl
{

void AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData, const int32 InIndex, const int32 InPortFlags, FStringOutputDevice& OutImportError)
{
	auto DoImportText = [&](const FString& InStringToImport)
	{
		InProp->ImportText(*InStringToImport, InProp->ContainerPtrToValuePtr<uint8>(InData, InIndex), InPortFlags, nullptr, &OutImportError);
	};

	bool bNeedsImport = true;

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
					FString StringToImport = EnumProp->Enum->GetEnumName(EnumEntryIndex);
					UE_LOG(LogDataTable, Warning, TEXT("Could not a find matching enum entry for '%s', but did find a matching display name. Will import using the enum entry corresponding to that display name ('%s')"), *InString, *StringToImport);
					DoImportText(StringToImport);
					bNeedsImport = false;
					break;
				}
			}
		}
	}

	if(bNeedsImport)
	{
		DoImportText(InString);
	}
}

void GetPropertyValueAsString(const UProperty* InProp, uint8* InData, const int32 InIndex, const int32 InPortFlags, FString& OutString)
{
	InProp->ExportText_InContainer(InIndex, OutString, InData, InData, nullptr, InPortFlags);
}

}

FString DataTableUtils::AssignStringToSingleProperty(const FString& InString, const UProperty* InProp, uint8* InData)
{
	FStringOutputDevice ImportError;
	if(InProp && IsSupportedTableProperty(InProp))
	{
		DataTableUtilsImpl::AssignStringToProperty(InString, InProp, InData, 0, PPF_None, ImportError);
	}

	FString Error = ImportError;
	return Error;
}

FString DataTableUtils::AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData)
{
	FStringOutputDevice ImportError;
	if(InProp && IsSupportedTableProperty(InProp))
	{
		if(InProp->ArrayDim == 1)
		{
			DataTableUtilsImpl::AssignStringToProperty(InString, InProp, InData, 0, PPF_None, ImportError);
		}
		else
		{
			if(InString.Len() >= 2 && InString[0] == '(' && InString[InString.Len() - 1] == ')')
			{
				// Trim the ( and )
				FString StringToSplit = InString.Mid(1, InString.Len() - 2);

				TArray<FString> Values;
				StringToSplit.ParseIntoArray(Values, TEXT(","), 1);

				if (InProp->ArrayDim != Values.Num())
				{
					UE_LOG(LogDataTable, Warning, TEXT("%s - Array is %d elements large, but we have %d values to import"), *InProp->GetName(), InProp->ArrayDim, Values.Num());
				}

				for (int32 Index = 0; Index < InProp->ArrayDim; ++Index)
				{
					if (Values.IsValidIndex(Index))
					{
						DataTableUtilsImpl::AssignStringToProperty(Values[Index], InProp, InData, Index, PPF_Delimited, ImportError);
					}
				}
			}
			else
			{
				UE_LOG(LogDataTable, Warning, TEXT("%s - Malformed array string. It must start with '(' and end with ')'"), *InProp->GetName());
			}
		}
	}

	FString Error = ImportError;
	return Error;
}

FString DataTableUtils::GetSinglePropertyValueAsString(const UProperty* InProp, uint8* InData)
{
	FString Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, 0, PPF_None, Result);
	}

	return Result;
}

FString DataTableUtils::GetPropertyValueAsString(const UProperty* InProp, uint8* InData)
{
	FString Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		if(InProp->ArrayDim == 1)
		{
			DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, 0, PPF_None, Result);
		}
		else
		{
			Result.AppendChar('(');

			for(int32 Index = 0; Index < InProp->ArrayDim; ++Index)
			{
				DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, Index, PPF_Delimited, Result);

				if((Index + 1) < InProp->ArrayDim)
				{
					Result.AppendChar(',');
				}
			}

			Result.AppendChar(')');
		}
	}

	return Result;
}

FText DataTableUtils::GetSinglePropertyValueAsText(const UProperty* InProp, uint8* InData)
{
	FText Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		FString ExportedString;
		DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, 0, PPF_PropertyWindow, ExportedString);

		Result = FText::FromString(MoveTemp(ExportedString));
	}

	return Result;
}

FText DataTableUtils::GetPropertyValueAsText(const UProperty* InProp, uint8* InData)
{
	FText Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		FString ExportedString;

		if(InProp->ArrayDim == 1)
		{
			DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, 0, PPF_PropertyWindow, ExportedString);
		}
		else
		{
			ExportedString.AppendChar('(');

			for(int32 Index = 0; Index < InProp->ArrayDim; ++Index)
			{
				DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, Index, PPF_PropertyWindow | PPF_Delimited, ExportedString);

				if((Index + 1) < InProp->ArrayDim)
				{
					ExportedString.AppendChar(',');
				}
			}

			ExportedString.AppendChar(')');
		}
		
		Result = FText::FromString(MoveTemp(ExportedString));
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
