// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "EnvQueryItemType_Actor.generated.h"

struct FEnvQueryContextData;

UCLASS()
class AIMODULE_API UEnvQueryItemType_Actor : public UEnvQueryItemType_ActorBase
{
	GENERATED_UCLASS_BODY()

	static AActor* GetValue(const uint8* RawData);
	static void SetValue(uint8* RawData, const AActor* Value);

	static void SetContextHelper(FEnvQueryContextData& ContextData, const AActor* SingleActor);
	static void SetContextHelper(FEnvQueryContextData& ContextData, const TArray<const AActor*>& MultipleActors);
	static void SetContextHelper(FEnvQueryContextData& ContextData, const TArray<AActor*>& MultipleActors);

	virtual FVector GetItemLocation(const uint8* RawData) const override;
	virtual FRotator GetItemRotation(const uint8* RawData) const override;
	virtual AActor* GetActor(const uint8* RawData) const override;
};
