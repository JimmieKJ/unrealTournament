// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnalyticsConversion.h"

/**
 * Struct to hold key/value pairs that will be sent as attributes along with analytics events.
 * All values are actually strings, but we provide a convenient constructor that relies on ToStringForAnalytics() to 
 * convert common types. 
 */
struct FAnalyticsEventAttribute
{
	FString AttrName;
	FString AttrValue;
	/** Default ctor since we declare a custom ctor. */
	FAnalyticsEventAttribute()
	{}

	#if !PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	/** copy ctor for platforms that don't implement defaulted functions properly. */
	FAnalyticsEventAttribute(const FAnalyticsEventAttribute& RHS)
		: AttrName(RHS.AttrName)
		, AttrValue(RHS.AttrValue)
	{}
	/** move ctor for platforms that don't implement defaulted functions properly. */
	FAnalyticsEventAttribute(FAnalyticsEventAttribute&& RHS)
		: AttrName(MoveTemp(RHS.AttrName))
		, AttrValue(MoveTemp(RHS.AttrValue))
	{}
	/** copy assignment ctor for platforms that don't implement defaulted functions properly. */
	FAnalyticsEventAttribute& operator=(const FAnalyticsEventAttribute& RHS)
	{
		AttrName = RHS.AttrName;
		AttrValue = RHS.AttrValue;
		return *this;
	}
	/** move assignment ctor for platforms that don't implement defaulted functions properly. */
	FAnalyticsEventAttribute& operator=(FAnalyticsEventAttribute&& RHS)
	{
		AttrName = MoveTemp(RHS.AttrName);
		AttrValue = MoveTemp(RHS.AttrValue);
		return *this;
	}
#endif

	/**
	 * Helper constructor to make an attribute from a name/value pair.
	 * 
	 * NameType 
	 * ValueType will be converted to a string via forwarding to ToStringForAnalytics.
	 *
	 * @param InName Name of the attribute. Will be forwarded to an FString constructor.
	 * @param InValue Value of the attribute. Will be forwarded to ToStringForAnalytics to convert to a string. 
	 * @return 
	 */
	template <typename NameType, typename ValueType>
	FAnalyticsEventAttribute(NameType&& InName, ValueType&& InValue)
		: AttrName(Forward<NameType>(InName))
		, AttrValue(AnalyticsConversion::ToString(Forward<ValueType>(InValue)))
	{}
};

/** Helper functions for MakeAnalyticsEventAttributeArray. */
namespace ImplMakeAnalyticsEventAttributeArray
{
	/** Recursion terminator. Empty list. */
	template <typename Allocator>
	inline void MakeArray(TArray<FAnalyticsEventAttribute, Allocator>& Attrs)
	{
	}

	/** Recursion terminator. Convert the key/value pair to analytics strings. */
	template <typename Allocator, typename KeyType, typename ValueType>
	inline void MakeArray(TArray<FAnalyticsEventAttribute, Allocator>& Attrs, KeyType&& Key, ValueType&& Value)
	{
		Attrs.Emplace(Forward<KeyType>(Key), Forward<ValueType>(Value));
	}

	/** recursively add the arguments to the array. */
	template <typename Allocator, typename KeyType, typename ValueType, typename...ArgTypes>
	inline void MakeArray(TArray<FAnalyticsEventAttribute, Allocator>& Attrs, KeyType&& Key, ValueType&& Value, ArgTypes&&...Args)
	{
		// pop off the top two args and recursively apply the rest.
		Attrs.Emplace(Forward<KeyType>(Key), Forward<ValueType>(Value));
		MakeArray(Attrs, Forward<ArgTypes>(Args)...);
	}
}

/** Helper to create an array of attributes using a single expression. There must be an even number of arguments, one for each key/value pair. */
template <typename Allocator = FDefaultAllocator, typename...ArgTypes>
inline TArray<FAnalyticsEventAttribute, Allocator> MakeAnalyticsEventAttributeArray(ArgTypes&&...Args)
{
	static_assert(sizeof...(Args) % 2 == 0, "Must pass an even number of arguments to MakeAnalyticsEventAttributeArray.");
	TArray<FAnalyticsEventAttribute, Allocator> Attrs;
	Attrs.Empty(sizeof...(Args) / 2);
	ImplMakeAnalyticsEventAttributeArray::MakeArray(Attrs, Forward<ArgTypes>(Args)...);
	return Attrs;
}

