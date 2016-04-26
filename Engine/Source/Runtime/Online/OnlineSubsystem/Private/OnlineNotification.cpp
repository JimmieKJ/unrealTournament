// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPrivatePCH.h"
#include "OnlineNotification.h"
#include "JsonObjectConverter.h"

FOnlineNotification::FOnlineNotification() :
Payload(nullptr),
ToUserId(nullptr),
FromUserId(nullptr)
{

}

// Treated as a system notification unless ToUserId is added
FOnlineNotification::FOnlineNotification(const FString& InTypeStr, const TSharedPtr<FJsonValue>& InPayload)
: TypeStr(InTypeStr)
, Payload(InPayload)
, ToUserId(nullptr)
, FromUserId(nullptr)
{

}

// Notification to a specific user.  FromUserId is optional
FOnlineNotification::FOnlineNotification(const FString& InTypeStr, const TSharedPtr<FJsonValue>& InPayload, TSharedPtr<const FUniqueNetId> InToUserId)
: TypeStr(InTypeStr)
, Payload(InPayload)
, ToUserId(InToUserId)
, FromUserId(nullptr)
{

}

FOnlineNotification::FOnlineNotification(const FString& InTypeStr, const TSharedPtr<FJsonValue>& InPayload, TSharedPtr<const FUniqueNetId> InToUserId, TSharedPtr<const FUniqueNetId> InFromUserId)
: TypeStr(InTypeStr)
, Payload(InPayload)
, ToUserId(InToUserId)
, FromUserId(InFromUserId)
{

}

bool FOnlineNotification::ParsePayload(UStruct* StructType, void* StructPtr) const
{
	if (StructType && StructPtr)
	{
		if (Payload.IsValid() && Payload->Type == EJson::Object)
		{
			return FJsonObjectConverter::JsonObjectToUStruct(Payload->AsObject().ToSharedRef(), StructType, StructPtr, 0, 0);
		}
	}
	return false;
}
