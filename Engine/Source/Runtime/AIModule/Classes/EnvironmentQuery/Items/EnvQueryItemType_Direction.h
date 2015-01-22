// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvQueryItemType_Direction.generated.h"

struct FEnvQueryContextData;

UCLASS()
class AIMODULE_API UEnvQueryItemType_Direction : public UEnvQueryItemType_VectorBase
{
	GENERATED_UCLASS_BODY()

	static FVector GetValue(const uint8* RawData);
	static void SetValue(uint8* RawData, const FVector& Value);

	static FRotator GetValueRot(const uint8* RawData);
	static void SetValueRot(uint8* RawData, const FRotator& Value);

	static void SetContextHelper(FEnvQueryContextData& ContextData, const FVector& SingleDirection);
	static void SetContextHelper(FEnvQueryContextData& ContextData, const FRotator& SingleRotation);
	static void SetContextHelper(FEnvQueryContextData& ContextData, const TArray<FVector>& MultipleDirections);
	static void SetContextHelper(FEnvQueryContextData& ContextData, const TArray<FRotator>& MultipleRotations);

	virtual FRotator GetItemRotation(const uint8* RawData) const override;
	virtual FString GetDescription(const uint8* RawData) const override;
};
