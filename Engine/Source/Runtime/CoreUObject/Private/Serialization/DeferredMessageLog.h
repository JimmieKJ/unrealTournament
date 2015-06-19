// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredMessgaeLog.h: Unreal async loading log.
=============================================================================*/

#pragma once

#include "MessageLog.h"

class FDeferredMessageLog
{
	FName LogCategory;

	static TMap<FName, TArray<TSharedRef<FTokenizedMessage>>*> Messages;

	void AddMessage(TSharedRef<FTokenizedMessage>& Message);

public:
	FDeferredMessageLog(const FName& InLogCategory);
	
	TSharedRef<FTokenizedMessage> Info(const FText& Message);
	TSharedRef<FTokenizedMessage> Warning(const FText& Message);
	TSharedRef<FTokenizedMessage> Error(const FText& Message);

	static void Flush();
};
