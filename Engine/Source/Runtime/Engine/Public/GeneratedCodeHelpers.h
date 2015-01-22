// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Common libraries
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"

// Special libraries
#include "Kismet/DataTableFunctionLibrary.h"

FORCEINLINE UClass* DynamicMetaCast(const UClass* DesiredClass, UClass* SourceClass)
{
	return ((SourceClass)->IsChildOf(DesiredClass)) ? SourceClass : NULL;
}

struct FCustomThunkTemplates
{
	//Replacements for CustomThunk functions from UKismetArrayLibrary

	template<typename T>
	static int32 Array_Add(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, const T& NewItem)
	{
		return UKismetArrayLibrary::GenericArray_Add(&TargetArray, ArrayProperty, &NewItem);
	}

	template<typename T>
	static int32 Array_AddUnique(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, const T& NewItem)
	{
		return UKismetArrayLibrary::GenericArray_AddUnique(&TargetArray, ArrayProperty, &NewItem);
	}

	template<typename T>
	static void Array_Shuffle(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty)
	{
		UKismetArrayLibrary::GenericArray_Shuffle(&TargetArray, ArrayProperty);
	}

	template<typename T>
	static void Array_Append(TArray<T>& TargetArray, const UArrayProperty* TargetArrayProperty, const TArray<T>& SourceArray, const UArrayProperty* SourceArrayProperty)
	{
		UKismetArrayLibrary::GenericArray_Append(&TargetArray, TargetArrayProperty, &SourceArray, SourceArrayProperty);
	}

	template<typename T>
	static void Array_Insert(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, const T& NewItem, int32 Index)
	{
		UKismetArrayLibrary::GenericArray_Insert(&TargetArray, ArrayProperty, &NewItem, Index);
	}

	template<typename T>
	static void Array_Remove(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, int32 IndexToRemove)
	{
		UKismetArrayLibrary::GenericArray_Remove(&TargetArray, ArrayProperty, IndexToRemove);
	}

	template<typename T>
	static int32 Array_Find(const TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, const T& ItemToFind)
	{
		return TargetArray.Find(ItemToFind);
	}

	template<typename T>
	static bool Array_RemoveItem(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, const T& Item)
	{
		return TargetArray.Remove(Item);
	}

	template<typename T>
	static void Array_Clear(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty)
	{
		UKismetArrayLibrary::GenericArray_Clear(&TargetArray, ArrayProperty);
	}

	template<typename T>
	static void Array_Resize(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, int32 Size)
	{
		UKismetArrayLibrary::GenericArray_Resize(&TargetArray, ArrayProperty, Size);
	}

	template<typename T>
	static int32 Array_Length(const TArray<T>& TargetArray, const UArrayProperty* ArrayProperty)
	{
		return UKismetArrayLibrary::GenericArray_Length(&TargetArray, ArrayProperty);
	}

	template<typename T>
	static int32 Array_LastIndex(const TArray<T>& TargetArray, const UArrayProperty* ArrayProperty)
	{
		return UKismetArrayLibrary::GenericArray_Length(&TargetArray, ArrayProperty);
	}

	template<typename T>
	static void Array_Get(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, int32 Index, T& Item)
	{
		UKismetArrayLibrary::GenericArray_Get(&TargetArray, ArrayProperty, Index, &Item);
	}

	template<typename T>
	static void Array_Set(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, int32 Index, const T& Item, bool bSizeToFit)
	{
		UKismetArrayLibrary::GenericArray_Set(&TargetArray, ArrayProperty, Index, &Item, bSizeToFit);
	}

	template<typename T>
	static bool Array_Contains(const TArray<T>& TargetArray, const UArrayProperty* ArrayProperty, const T& ItemToFind)
	{
		return Array_Find(TargetArray, ArrayProperty, ItemToFind) > 0;
	}

	template<typename T>
	static void SetArrayPropertyByName(UObject* Object, FName PropertyName, TArray<T>& Value)
	{
		UKismetArrayLibrary::GenericArray_SetArrayPropertyByName(Object, PropertyName, &Value);
	}

	//Replacements for CustomThunk functions from UDataTableFunctionLibrary

	template<typename T>
	static bool GetDataTableRowFromName(UDataTable* Table, FName RowName, T& OutRow)
	{
		return UDataTableFunctionLibrary::Generic_GetDataTableRowFromName(Table, RowName, &OutRow);
	}

	//Replacements for CustomThunk functions from UKismetSystemLibrary

	template<typename T>
	static void SetStructurePropertyByName(UObject* Object, FName PropertyName, const T& Value)
	{
		return UKismetSystemLibrary::Generic_SetStructurePropertyByName(Object, PropertyName, &Value);
	}
};