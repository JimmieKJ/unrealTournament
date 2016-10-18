// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvQueryItemType_Point.generated.h"

struct FEnvQueryContextData;

UCLASS()
class AIMODULE_API UEnvQueryItemType_Point : public UEnvQueryItemType_VectorBase
{
	GENERATED_UCLASS_BODY()

	static FVector GetValue(const uint8* RawData);
	static void SetValue(uint8* RawData, const FVector& Value);
	static FNavLocation GetNavValue(const uint8* RawData);
	static void SetNavValue(uint8* RawData, const FNavLocation& Value);

	static void SetContextHelper(FEnvQueryContextData& ContextData, const FVector& SinglePoint);
	static void SetContextHelper(FEnvQueryContextData& ContextData, const TArray<FVector>& MultiplePoints);

	virtual FVector GetItemLocation(const uint8* RawData) const override;
	virtual FNavLocation GetItemNavLocation(const uint8* RawData) const;

	/** Update location data in existing item */
	virtual void SetItemNavLocation(uint8* RawData, const FNavLocation& Value) const;
};

// a specialization to support saving locations with navigation data already gathered
template<>
AIMODULE_API void FEnvQueryInstance::AddItemData<UEnvQueryItemType_Point, FNavLocation>(FNavLocation ItemValue);