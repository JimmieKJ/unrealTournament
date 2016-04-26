// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef ENABLE_COOK_STATS
#define ENABLE_COOK_STATS WITH_EDITOR
#endif

#if ENABLE_COOK_STATS
#include "Core.h"

/**
 * Centralizes the system to gather stats from a cook that need to be collected at the core/engine level.
 * Essentially, add a delegate to the CookStatsCallbacks member. When a cook a complete that is configured to use stats (ENABLE_COOK_STATS),
 * it will broadcast this delegate, which, when called, gives you a TFunction to call to report each of your stats.
 * 
 * For simplicity, FAutoRegisterCallback is provided to auto-register your callback on startup. Usage is like:
 * 
 * static void ReportCookStats(AddStatFuncRef AddStat)
 * {
 *		AddStat("MySubsystem.Event1", CreateKeyValueArray("Attr1", "Value1", "Attr2", "Value2" ... );
 *		AddStat("MySubsystem.Event2", CreateKeyValueArray("Attr1", "Value1", "Attr2", "Value2" ... );
 * }
 * 
 * FAutoRegisterCallback GMySubsystemCookRegistration(&ReportCookStats);
 * 
 * When a cook is complete, your callback will be called, and the stats will be either logged, sent to an
 * analytics provider, logged to an external file, etc. You don't care how they are added, you just call 
 * AddStat for each stat you want to add.
 */
class FCookStatsManager
{
public:
	/** Every stat is essentially a name followed by an array of key/value "attributes" associated with the stat. */
	struct KeyValue
	{
		FString Key;
		FString Value;
		KeyValue(const FString& InKey, const FString& InValue) : Key(InKey), Value(InValue) {}
	};
	/** When you register a callback for reporting your stats, you will be given a TFunctionRef to call to add stats. This is the signature of the function you will call. */
	typedef TFunctionRef<void(const FString& StatName, const TArray<KeyValue>& StatAttributes)> AddStatFuncRef;

	/** To register a callback for reporting stats, create an instance of this delegate type and add it to the delegate variable below. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FGatherCookStatsDelegate, AddStatFuncRef);

	/** Use this to register a callback to gather cook stats for a translation unit. When a cook is finished, this delegate will be fired. */
	static CORE_API FGatherCookStatsDelegate CookStatsCallbacks;

	/** Called after the cook is finished to gather the stats. */
	static CORE_API void LogCookStats(AddStatFuncRef AddStat);

	/** Helper struct to auto-register your STATIC FUNCTION with CookStatsCallbacks */
	struct FAutoRegisterCallback
	{
		template<typename Func>
		explicit FAutoRegisterCallback(Func Callback)
		{
			CookStatsCallbacks.AddLambda(Callback);
		}
	};

	//-------------------------------------------------------------------------
	// Helper to initialize an array of KeyValues on one line.
	//-------------------------------------------------------------------------

	/** Helper to create an array of KeyValues using a single expression. If there is an odd number of arguments, the last value is considered to be an empty string. */
	template <typename...ArgTypes>
	static TArray<FCookStatsManager::KeyValue> CreateKeyValueArray(ArgTypes&&...Args)
	{
		TArray<FCookStatsManager::KeyValue> Attrs;
		Attrs.Empty(sizeof...(Args) / 2);
		ImplCreateKeyValueArray(Attrs, Args...);
		return Attrs;
	}
private:
	/** ToString abstraction that can convert anything into a string. Differs from LexicalConversion in that it is legal to pass a string or TCHAR* to it. */
	static const FString& ToString(const FString& InStr) { return InStr; }
	static FString ToString(const TCHAR* InStr) { return FString(InStr); }
	template <typename T> static FString ToString(T&&Val) { return LexicalConversion::ToString(Forward<T>(Val)); }

	template <typename KeyType>
	static void ImplCreateKeyValueArray(TArray<FCookStatsManager::KeyValue>& Attrs, KeyType&& Key)
	{
		Attrs.Emplace(ToString(Forward<KeyType>(Key)), TEXT(""));
	}

	template <typename KeyType, typename ValueType>
	static void ImplCreateKeyValueArray(TArray<FCookStatsManager::KeyValue>& Attrs, KeyType&& Key, ValueType&& Value)
	{
		Attrs.Emplace(ToString(Forward<KeyType>(Key)), ToString(Forward<ValueType>(Value)));
	}

	template <typename KeyType, typename ValueType, typename...ArgTypes>
	static void ImplCreateKeyValueArray(TArray<FCookStatsManager::KeyValue>& Attrs, KeyType&& Key, ValueType&& Value, ArgTypes&&...Args)
	{
		Attrs.Emplace(ToString(Forward<KeyType>(Key)), ToString(Forward<ValueType>(Value)));
		ImplCreateKeyValueArray(Attrs, Forward<ArgTypes>(Args)...);
	}

	};

/** any expression inside this block will be compiled out if ENABLE_COOK_STATS is not true. Shorthand for the multi-line #if ENABLE_COOK_STATS guard. */
#define COOK_STAT(expr) expr
#else
#define COOK_STAT(expr)
#endif
