// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"

class FJsonValue;
class UStruct;

/** Notification object, used to send messages between systems */
struct ONLINESUBSYSTEM_API FOnlineNotification
{
public:
	FOnlineNotification() ;

	// Treated as a system notification unless ToUserId is added
	FOnlineNotification(const FString& InTypeStr, const TSharedPtr<FJsonValue>& InPayload);

	// Notification to a specific user.  FromUserId is optional
	FOnlineNotification(const FString& InTypeStr, const TSharedPtr<FJsonValue>& InPayload, TSharedPtr<const FUniqueNetId> InToUserId);

	FOnlineNotification(const FString& InTypeStr, const TSharedPtr<FJsonValue>& InPayload, TSharedPtr<const FUniqueNetId> InToUserId, TSharedPtr<const FUniqueNetId> InFromUserId);

public:

	/**
	 * Templated helper for payload parsing
	 * @param PayloadOut The structure to read the payload into
	 */
	template<typename USTRUCT_T>
	FORCEINLINE bool ParsePayload(USTRUCT_T& PayloadOut, const FString& ExpectedType) const
	{
		if (TypeStr != ExpectedType)
		{
			return false;
		}
		return ParsePayload(USTRUCT_T::StaticStruct(), &PayloadOut);
	}

	/**
	 * Parse a payload and assume there is a static const TypeStr member to use
	 */
	template<typename USTRUCT_T>
	FORCEINLINE bool ParsePayload(USTRUCT_T& PayloadOut) const
	{
		return ParsePayload(PayloadOut, USTRUCT_T::TypeStr);
	}

	/**
	 * Parse out Payload into the provided UStruct
	 */
	bool ParsePayload(UStruct* StructType, void* StructPtr) const;

public:

	/** A string defining the type of this notification, used to determine how to parse the payload */
	FString TypeStr;

	/** The payload of this notification */
	TSharedPtr<FJsonValue> Payload;

	/** User to deliver the notification to.  Can be null for system notifications. */
	TSharedPtr<const FUniqueNetId> ToUserId;

	/** User who sent the notification, optional. */
	TSharedPtr<const FUniqueNetId> FromUserId;
};
