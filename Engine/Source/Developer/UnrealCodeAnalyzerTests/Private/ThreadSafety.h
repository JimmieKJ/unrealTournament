// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ThreadSafety.generated.h"

UCLASS()
class UObjectDerivedThreadSafetyTest : public UObject
{
	GENERATED_BODY()

public:
	UObjectDerivedThreadSafetyTest();

	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
};