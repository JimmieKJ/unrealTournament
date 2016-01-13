// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Commandlets/Commandlet.h"
#include "UTGenerateRedirectsCommandlet.generated.h"

UCLASS()
class UUTGenerateRedirectsCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface

	FString MD5Sum(const TArray<uint8>& Data);
};