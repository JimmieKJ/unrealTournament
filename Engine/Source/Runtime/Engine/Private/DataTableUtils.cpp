// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "DataTableUtils.h"

FString DataTableUtils::AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData)
{
	FStringOutputDevice ImportError;
	if(InProp != NULL && IsSupportedTableProperty(InProp))
	{
		InProp->ImportText(*InString, InProp->ContainerPtrToValuePtr<uint8>(InData), PPF_None, NULL, &ImportError);
	}

	FString Error = ImportError;
	return Error;
}

FString DataTableUtils::GetPropertyValueAsString(const UProperty* InProp, uint8* InData)
{
	FString Result(TEXT(""));

	if(InProp != NULL && IsSupportedTableProperty(InProp))
	{
		InProp->ExportText_InContainer(0, Result, InData, InData, NULL, PPF_None);
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
