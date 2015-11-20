// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogDataTable, Log, All);

namespace DataTableUtils
{
	/** Util to assign a value (given as a string) to a struct property. */
	ENGINE_API FString AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData);

	/** Util to get a property as a string */
	ENGINE_API FString GetPropertyValueAsString(const UProperty* InProp, uint8* InData);

	/** Util to get a property as text (this will use the display name of the value where available - use GetPropertyValueAsString if you need an internal identifier) */
	ENGINE_API FText GetPropertyValueAsText(const UProperty* InProp, uint8* InData);

	/** Util to get all property names from a struct */
	ENGINE_API TArray<FName> GetStructPropertyNames(UStruct* InStruct);

	/** Util that removes invalid chars and then make an FName */
	ENGINE_API FName MakeValidName(const FString& InString);

	/** Util to see if this property is supported in a row struct. */
	ENGINE_API bool IsSupportedTableProperty(const UProperty* InProp);

	/** Util to get the friendly display name of a given property */
	ENGINE_API FString GetPropertyDisplayName(const UProperty* Prop, const FString& DefaultName);
}
