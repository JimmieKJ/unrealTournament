// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"

/** Helpers for converting various common types to strings that analytics providers can consume. */
namespace AnalyticsConversion
{
	/** Identity conversion for strings. Complete passthrough. */
	inline const FString& ToString(const FString& Str)
	{
		return Str;
	}

	/** Identity conversion for strings. Move support. */
	inline FString&& ToString(FString&& Str)
	{
		return MoveTemp(Str);
	}

	/** Identity conversion for strings. char-array support. */
	inline FString ToString(const ANSICHAR* Str)
	{
		return Str;
	}

	/** Identity conversion for strings. char-array support. */
	inline FString ToString(const WIDECHAR* Str)
	{
		return Str;
	}

	/** Bool conversion. */
	inline FString ToString(bool Value)
	{
		return Value ? TEXT("true") : TEXT("false");
	}

	/** Guid conversion. */
	inline FString ToString(FGuid Value)
	{
		return Value.ToString();
	}

	/** Double conversion. LexicalConversion is broken to doubles (won't use SanitizeFloat), so overload this directly. */
	inline FString ToString(double Value)
	{
		return FString::SanitizeFloat(Value);
	}


	/** Lexical conversion. Allow any type that we have a LexicalConversion for. */
	template <typename T>
	inline typename TEnableIf<TIsArithmetic<T>::Value, FString>::Type ToString(T Value)
	{
		return LexicalConversion::ToSanitizedString(Value);
	}

	/** Array conversion. Creates comma-separated list. */
	template <typename T, typename AllocatorType>
	FString ToString(const TArray<T, AllocatorType>& ValueArray)
	{
		FString Result;
		// Serialize the array into "value1,value2,..." format
		for (const T& Value : ValueArray)
		{
			Result += AnalyticsConversion::ToString(Value);
			Result += TEXT(",");
		}
		// Remove the trailing comma (LeftChop will ensure an empty container won't crash here).
		Result = Result.LeftChop(1);
		return Result;
	}

	/** Map conversion. Creates comma-separated list. Creates comma-separated list with colon-separated key:value pairs. */
	template<typename KeyType, typename ValueType, typename Allocator, typename KeyFuncs>
	FString ToString(const TMap<KeyType, ValueType, Allocator, KeyFuncs>& ValueMap)
	{
		FString Result;
		// Serialize the map into "key:value,..." format
		for (auto& KVP : ValueMap)
		{
			Result += AnalyticsConversion::ToString(KVP.Key);
			Result += TEXT(":");
			Result += AnalyticsConversion::ToString(KVP.Value);
			Result += TEXT(",");
		}
		// Remove the trailing comma (LeftChop will ensure an empty container won't crash here).
		Result = Result.LeftChop(1);
		return Result;
	}
}

