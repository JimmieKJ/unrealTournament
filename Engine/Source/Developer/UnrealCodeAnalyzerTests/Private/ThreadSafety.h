// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ThreadSafety.generated.h"

class FArchive;

UCLASS()
class UObjectDerivedThreadSafetyTest : public UObject
{
	GENERATED_BODY()

public:
	UObjectDerivedThreadSafetyTest();

	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
};
