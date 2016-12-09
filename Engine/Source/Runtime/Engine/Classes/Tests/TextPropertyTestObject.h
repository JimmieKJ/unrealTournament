// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.






#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "TextPropertyTestObject.generated.h"
UCLASS()
class UTextPropertyTestObject : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	FText DefaultedText;
	UPROPERTY()
	FText UndefaultedText;
	UPROPERTY()
	FText TransientText;
};
