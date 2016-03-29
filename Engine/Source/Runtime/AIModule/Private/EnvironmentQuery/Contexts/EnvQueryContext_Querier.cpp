// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "GameFramework/Controller.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"

UEnvQueryContext_Querier::UEnvQueryContext_Querier(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UEnvQueryContext_Querier::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	AActor* QueryOwner = Cast<AActor>(QueryInstance.Owner.Get());
	UE_CVLOG(GET_AI_CONFIG_VAR(bAllowControllersAsEQSQuerier) == false && Cast<AController>(QueryOwner) != nullptr, QueryOwner, LogEQS, Warning, TEXT("Using Controller as query's owner is dangerous since Controller's location is usually not what you expect it to be!"));
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, QueryOwner);
}
