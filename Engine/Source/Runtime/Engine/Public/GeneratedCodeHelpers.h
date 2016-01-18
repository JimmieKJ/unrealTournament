// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// DO NOT INCLUDE THIS HEADER!
// THIS FILE SHOULD BE USED ONLY BY AUTOMATICALLY GENERATED CODE. 

// Common includes
#include "UObject/Stack.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/BlueprintGeneratedClass.h"

// Common libraries
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"

// Special libraries
#include "Kismet/DataTableFunctionLibrary.h"

//For DOREPLIFETIME macros
#include "Net/UnrealNetwork.h"

template<class NativeType>
inline NativeType* NoNativeCast(UClass* NoNativeClass, UObject* Object)
{
	check(NoNativeClass);
	check(!Object || (nullptr != Cast<NativeType>(Object)));
	return (Object && Object->IsA(NoNativeClass)) ? ((NativeType*)Object) : nullptr;
}

inline UClass* DynamicMetaCast(const UClass* DesiredClass, UClass* SourceClass)
{
	return ((SourceClass)->IsChildOf(DesiredClass)) ? SourceClass : NULL;
}

inline bool IsValid(const FScriptInterface& Test)
{
	return IsValid(Test.GetObject()) && (nullptr != Test.GetInterface());
}

template<class TEnum>
inline uint8 EnumToByte(TEnumAsByte<TEnum> Val)
{
	return static_cast<uint8>(Val.GetValue());
}

template<class T>
inline const T* GetDefaultValueSafe(UClass* Class)
{
	return IsValid(Class) ? GetDefault<T>(Class) : nullptr;
}

struct FCustomThunkTemplates
{
private:
	static void ExecutionMessage(const TCHAR* Message, ELogVerbosity::Type Verbosity)
	{
		FFrame::KismetExecutionMessage(Message, Verbosity);
	}

	template<typename T>
	static int32 LastIndexForLog(const TArray<T>& TargetArray)
	{
		const int32 ArraySize = TargetArray.Num();
		return (ArraySize > 0) ? (ArraySize - 1) : 0;
	}

public:
	//Replacements for CustomThunk functions from UKismetArrayLibrary

	template<typename T, typename U>
	static int32 Array_Add(TArray<T>& TargetArray, const U& NewItem)
	{
		return TargetArray.Add(NewItem);
	}

	template<typename T, typename U>
	static int32 Array_AddUnique(TArray<T>& TargetArray, const U& NewItem)
	{
		return TargetArray.AddUnique(NewItem);
	}

	template<typename T>
	static void Array_Shuffle(TArray<T>& TargetArray, const UArrayProperty* ArrayProperty)
	{
		int32 LastIndex = TargetArray.Num() - 1;
		for (int32 i = 0; i < LastIndex; ++i)
		{
			int32 Index = FMath::RandRange(0, LastIndex);
			if (i != Index)
			{
				TargetArray.Swap(i, Index);
			}
		}
	}

	template<typename T, typename U>
	static void Array_Append(TArray<T>& TargetArray, const TArray<U>& SourceArray)
	{
		TargetArray.Append(SourceArray);
	}

	template<typename T, typename U>
	static void Array_Insert(TArray<T>& TargetArray, const U& NewItem, int32 Index)
	{
		if ((Index >= 0) && (Index <= TargetArray.Num()))
		{
			TargetArray.Insert(NewItem, Index);
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to insert an item into array out of bounds [%d/%d]!"), Index, LastIndexForLog(TargetArray)), ELogVerbosity::Warning);
		}
	}

	template<typename T>
	static void Array_Remove(TArray<T>& TargetArray, int32 IndexToRemove)
	{
		if (TargetArray.IsValidIndex(IndexToRemove))
		{
			TargetArray.RemoveAt(IndexToRemove);
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to remove an item from an invalid index from array [%d/%d]!"), IndexToRemove, LastIndexForLog(TargetArray)), ELogVerbosity::Warning);
		}
	}

	template<typename T, typename U>
	static int32 Array_Find(const TArray<T>& TargetArray, const U& ItemToFind)
	{
		return TargetArray.Find(ItemToFind);
	}

	template<typename T, typename U>
	static bool Array_RemoveItem(TArray<T>& TargetArray, const U& Item)
	{
		return TargetArray.Remove(Item) != 0;
	}

	template<typename T>
	static void Array_Clear(TArray<T>& TargetArray)
	{
		TargetArray.Empty();
	}

	template<typename T>
	static void Array_Resize(TArray<T>& TargetArray, int32 Size)
	{
		if (Size >= 0)
		{
			TargetArray.SetNum(Size);
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to resize an array using negative size: Size = %d!"), Size), ELogVerbosity::Warning);
		}
	}

	template<typename T>
	static int32 Array_Length(const TArray<T>& TargetArray)
	{
		return TargetArray.Num();
	}

	template<typename T>
	static int32 Array_LastIndex(const TArray<T>& TargetArray)
	{
		return TargetArray.Num() - 1;
	}

	template<typename T, typename U>
	static void Array_Get(TArray<T>& TargetArray, int32 Index, U& Item)
	{
		if (TargetArray.IsValidIndex(Index))
		{
			Item = TargetArray[Index];
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to get an item from array out of bounds [%d/%d]!"), Index, LastIndexForLog(TargetArray)), ELogVerbosity::Warning);
			Item = U{};
		}
	}

	template<typename T, typename U>
	static void Array_Set(TArray<T>& TargetArray, int32 Index, const U& Item, bool bSizeToFit)
	{
		if (!TargetArray.IsValidIndex(Index) && bSizeToFit && (Index >= 0))
		{
			TargetArray.SetNum(Index + 1);
		}

		if (TargetArray.IsValidIndex(Index))
		{
			TargetArray[Index] = Item;
		}
		else
		{
			ExecutionMessage(*FString::Printf(TEXT("Attempted to set an invalid index on array [%d/%d]!"), Index, LastIndexForLog(TargetArray)), ELogVerbosity::Warning);
		}
	}

	template<typename T, typename U>
	static bool Array_Contains(const TArray<T>& TargetArray, const U& ItemToFind)
	{
		return TargetArray.Contains(ItemToFind);
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
	static void StackTrace()
	{
		ExecutionMessage(TEXT("Native code cannot generate a stack trace."), ELogVerbosity::Log);
	}
	
	template<typename T>
	static void SetStructurePropertyByName(UObject* Object, FName PropertyName, const T& Value)
	{
		return UKismetSystemLibrary::Generic_SetStructurePropertyByName(Object, PropertyName, &Value);
	}

	static void SetCollisionProfileNameProperty(UObject* Object, FName PropertyName, const FCollisionProfileName& Value)
	{
		return UKismetSystemLibrary::Generic_SetStructurePropertyByName(Object, PropertyName, &Value);
	}

	// Replacements for CustomThunk functions from UBlueprintFunctionLibrary
	static FStringAssetReference MakeStringAssetReference(const FString& AssetLongPathname)
	{
		FStringAssetReference Ref(AssetLongPathname);
		if (!AssetLongPathname.IsEmpty() && !Ref.IsValid())
		{
			ExecutionMessage(*FString::Printf(TEXT("Asset path \"%s\" not valid. Only long path name is allowed."), *AssetLongPathname), ELogVerbosity::Error);
			return FStringAssetReference();
		}

		return Ref;
	}

	// Replacements for CustomThunk functions from KismetMathLibrary
	static float Divide_FloatFloat(float A, float B)
	{
		if (B == 0.f)
		{
			ExecutionMessage(TEXT("Divide by zero"), ELogVerbosity::Warning);
			return 0.0f;
		}
		return UKismetMathLibrary::GenericDivide_FloatFloat(A, B);
	}

	static float Percent_FloatFloat(float A, float B)
	{
		if (B == 0.f)
		{
			ExecutionMessage(TEXT("Modulo by zero"), ELogVerbosity::Warning);
			return 0.0f;
		}

		return UKismetMathLibrary::GenericPercent_FloatFloat(A, B);
	}
};

template<typename IndexType, typename ValueType>
struct TSwitchPair
{
	const IndexType& IndexRef;
	ValueType& ValueRef;

	TSwitchPair(const IndexType& InIndexRef, ValueType& InValueRef)
		: IndexRef(InIndexRef)
		, ValueRef(InValueRef)
	{}
};

template<typename IndexType, typename ValueType>
struct TSwitchPair<IndexType, ValueType*>
{
	const IndexType& IndexRef;
	ValueType*& ValueRef;

	template<typename DerivedType>
	TSwitchPair(const IndexType& InIndexRef, DerivedType*& InValueRef)
		: IndexRef(InIndexRef)
		, ValueRef((ValueType*&)InValueRef)
	{
		static_assert(TPointerIsConvertibleFromTo<DerivedType, ValueType>::Value, "Incompatible pointers");
	}
};

#if PLATFORM_COMPILER_HAS_VARIADIC_TEMPLATES
template<typename IndexType, typename ValueType>
ValueType& TSwitchValue(const IndexType& CurrentIndex, ValueType& DefaultValue, int OptionsNum)
{
	return DefaultValue;
}

template<typename IndexType, typename ValueType, typename Head, typename... Tail>
ValueType& TSwitchValue(const IndexType& CurrentIndex, ValueType& DefaultValue, int OptionsNum, Head HeadOption, Tail... TailOptions)
{
	if (CurrentIndex == HeadOption.IndexRef)
	{
		return HeadOption.ValueRef;
	}
	return TSwitchValue<IndexType, ValueType, Tail...>(CurrentIndex, DefaultValue, OptionsNum, TailOptions...);
}
#else //PLATFORM_COMPILER_HAS_VARIADIC_TEMPLATES
template<typename IndexType, typename ValueType>
ValueType& TSwitchValue(const IndexType& CurrentIndex, ValueType& DefaultValue, int OptionsNum, ...)
{
	typedef TSwitchPair < IndexType, ValueType > OptionType;

	ValueType* SelectedValuePtr = nullptr;

	va_list Options;
	va_start(Options, OptionsNum);
	for (int OptionIt = 0; OptionIt < OptionsNum; ++OptionIt)
	{
		OptionType Option = va_arg(Options, OptionType);
		if (Option.IndexRef == CurrentIndex)
		{
			SelectedValuePtr = &Option.ValueRef;
			break;
		}
	}
	va_end(Options);

	return SelectedValuePtr ? *SelectedValuePtr : DefaultValue;
}
#endif //PLATFORM_COMPILER_HAS_VARIADIC_TEMPLATES

// Base class for wrappers for unconverted BlueprintGeneratedClasses
template<class NativeType>
struct FUnconvertedWrapper
{
	NativeType* __Object;

	FUnconvertedWrapper(const UObject* InObject) : __Object(CastChecked<NativeType>(const_cast<UObject*>(InObject))) {}

	operator NativeType*() const { return __Object; }

	UClass* GetClass() const
	{
		check(__Object);
		return __Object->GetClass();
	}
};

template<typename T>
struct TArrayCaster
{
	TArray<T*> Val;
	TArray<T*>& ValRef;

	TArrayCaster(TArray<T*>& InArr) 
		: Val()
		, ValRef(InArr)
	{}

	TArrayCaster(TArray<T*>&& InArr) 
		: Val(MoveTemp(InArr))
		, ValRef(Val)
	{}

	template<typename U>
	TArray<U*>& Get()
	{
		static_assert(sizeof(T*) == sizeof(U*), "Incompatible pointers");
		return *reinterpret_cast<TArray<U*>*>(&ValRef);
	}
};