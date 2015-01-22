// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvQueryItemType_VectorBase.generated.h"

struct FBlackboardKeySelector;
class UBlackboardComponent;

UCLASS(Abstract)
class AIMODULE_API UEnvQueryItemType_VectorBase : public UEnvQueryItemType
{
	GENERATED_UCLASS_BODY()

	virtual FVector GetItemLocation(const uint8* RawData) const;
	virtual FRotator GetItemRotation(const uint8* RawData) const;

	virtual void AddBlackboardFilters(FBlackboardKeySelector& KeySelector, UObject* FilterOwner) const override;
	virtual bool StoreInBlackboard(FBlackboardKeySelector& KeySelector, UBlackboardComponent* Blackboard, const uint8* RawData) const override;
	virtual FString GetDescription(const uint8* RawData) const override;
};
