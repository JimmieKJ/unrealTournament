// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a type map for messages.
 */
class FMessageTypeMap
{
	TMap<FString, UScriptStruct*> TypeMap;
	FCriticalSection SyncObj;

public:
	static COREUOBJECT_API FMessageTypeMap MessageTypeMap;

	void Add(const FString& Name, UScriptStruct* Type)
	{
		SyncObj.Lock();
		TypeMap.Add(Name, Type);
		SyncObj.Unlock();
	}

	UScriptStruct* Find(const FString& Name)
	{
		SyncObj.Lock();
		UScriptStruct* Type = NULL;
		if (TypeMap.Find(Name))
		{
			Type = *TypeMap.Find(Name);
		}
		SyncObj.Unlock();
		return Type;
	}
};
