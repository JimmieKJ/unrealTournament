// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"

/** 
 * Struct to hold key/value pairs that will be sent as attributes along with analytics events.
 * All values are actually strings, but we provide convenient conversion functions for most basic types. 
 */
struct FAnalyticsEventAttribute
{
	FString AttrName;
	FString AttrValue;

	FAnalyticsEventAttribute(const FString& InName, const FString& InValue)
		:AttrName(InName)
		,AttrValue(InValue)
	{
	}

	FAnalyticsEventAttribute(const FString& InName, const TCHAR* InValue)
		:AttrName(InName)
		,AttrValue(InValue)
	{
	}

	FAnalyticsEventAttribute(const FString& InName, bool InValue)
		:AttrName(InName)
		,AttrValue(InValue ? TEXT("true") : TEXT("false"))
	{
	}

	FAnalyticsEventAttribute(const FString& InName,FGuid InValue)
		:AttrName(InName)
		,AttrValue(InValue.ToString())
	{
	}

	// Allow any type that we have a valid format specifier for (disallowing implicit conversions).
	template <typename T>
	FAnalyticsEventAttribute(const FString& InName, T InValue)
		:AttrName(InName)
		, AttrValue(TTypeToString<T>::ToString(InValue))
	{
	}

	// Allow arrays to be used as values formatted as comma delimited lists
	template <typename T>
	FAnalyticsEventAttribute(const FString& InName, const TArray<T>& InValueArray)
		:AttrName(InName)
		,AttrValue(FString())
	{
		// Serialize the array into "value1,value2,..." format
		for (const T& Value : InValueArray)
		{
			AttrValue += TTypeToString<T>::ToString(Value) + ",";
		}

		// Remove the trailing comma
		AttrValue = AttrValue.LeftChop(1);
	}

	// special case maps of FString->FString since TTypeToString (ironically) doesn't support FString
	FAnalyticsEventAttribute(const FString& InName, const TMap<FString, FString>& InValueMap)
		:AttrName(InName)
		, AttrValue(FString())
	{
		// Serialize the map into "key:value,..." format
		for (auto& KVP : InValueMap)
		{
			AttrValue += KVP.Key + ":" + KVP.Value + ",";
		}

		// Remove the trailing comma
		AttrValue = AttrValue.LeftChop(1);
	}


	// Allow maps to be used as values formatted as comma delimited key-value pairs
	template <typename T>
	FAnalyticsEventAttribute(const FString& InName, const TMap<FString, T>& InValueMap)
		:AttrName(InName)
		,AttrValue(FString())
	{
		// Serialize the map into "key:value,..." format
		for (auto& KVP : InValueMap)
		{
			AttrValue += KVP.Key + ":" + TTypeToString<T>::ToString(KVP.Value) + ",";
		}

		// Remove the trailing comma
		AttrValue = AttrValue.LeftChop(1);
	}
};
