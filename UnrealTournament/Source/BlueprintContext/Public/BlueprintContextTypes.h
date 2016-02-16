// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintContextTypes.generated.h"

UENUM( BlueprintType )
enum class EPartyMemberState : uint8
{
	Solo,
	InParty,
};

/**
 * This is a simple wrapper for TEnableIf that simplifies use
 *
 * usage:
 * TTEnableIf< true, int32 > Test();
 * void Test2( TTEnableIf< false, uint32 > Value );
 */
template <bool TCondition, typename TType = void>
using TTEnableIf                          = typename TEnableIf<TCondition, TType>::Type;

#if !defined( __clang__ ) && !defined( __ORBIS__ )
#define LOG( Category, Level, Text, ... )                                              \
	{                                                                                  \
		const FString& LogText = FString::Printf( Text, ##__VA_ARGS__ );               \
		UE_LOG( Category, Level, TEXT( "[%s]: %s" ), TEXT( __FUNCTION__ ), *LogText ); \
	}

#define LOG_IF( Condition, Category, Level, Text, ... )                                            \
	{                                                                                              \
		const FString& LogText = FString::Printf( Text, ##__VA_ARGS__ );                           \
		UE_CLOG( Condition, Category, Level, TEXT( "[%s]: %s" ), TEXT( __FUNCTION__ ), *LogText ); \
	}
#else
#define LOG( Category, Level, Text, ... )                                          \
	{                                                                              \
		const FString& LogText = FString::Printf( Text, ##__VA_ARGS__ );           \
		UE_LOG( Category, Level, TEXT( "[%s]: %s" ), ANSI_TO_TCHAR(__func__), *LogText ); \
	}

#define LOG_IF( Condition, Category, Level, Text, ... )                                        \
	{                                                                                          \
		const FString& LogText = FString::Printf( Text, ##__VA_ARGS__ );                       \
		UE_CLOG( Condition, Category, Level, TEXT( "[%s]: %s" ), ANSI_TO_TCHAR(__func__), *LogText ); \
	}
#endif