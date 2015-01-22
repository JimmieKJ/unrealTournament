// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BlueprintNodeHelpers.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_BlueprintBase.h"
#include "EnvironmentQuery/Items/EnvQueryAllItemTypes.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/EnvQueryManager.h"

namespace
{
	FORCEINLINE bool DoesImplementBPFunction(FName FuncName, const UObject* Ob, const UClass* StopAtClass)
	{
		return (Ob->GetClass()->FindFunctionByName(FuncName)->GetOuter() != StopAtClass);
	}
}

UEnvQueryContext_BlueprintBase::UEnvQueryContext_BlueprintBase(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer), CallMode(UEnvQueryContext_BlueprintBase::InvalidCallMode)
{
	UClass* StopAtClass = UEnvQueryContext_BlueprintBase::StaticClass();	
	bool bImplementsProvideSingleActor = DoesImplementBPFunction(GET_FUNCTION_NAME_CHECKED(UEnvQueryContext_BlueprintBase, ProvideSingleActor), this, StopAtClass);
	bool bImplementsProvideSingleLocation = DoesImplementBPFunction(GET_FUNCTION_NAME_CHECKED(UEnvQueryContext_BlueprintBase, ProvideSingleLocation), this, StopAtClass);
	bool bImplementsProvideActorSet = DoesImplementBPFunction(GET_FUNCTION_NAME_CHECKED(UEnvQueryContext_BlueprintBase, ProvideActorsSet), this, StopAtClass);
	bool bImplementsProvideLocationsSet = DoesImplementBPFunction(GET_FUNCTION_NAME_CHECKED(UEnvQueryContext_BlueprintBase, ProvideLocationsSet), this, StopAtClass);

	int32 ImplementationsCount = 0;
	if (bImplementsProvideSingleActor)
	{
		++ImplementationsCount;
		CallMode = SingleActor;
	}

	if (bImplementsProvideSingleLocation)
	{
		++ImplementationsCount;
		CallMode = SingleLocation;
	}

	if (bImplementsProvideActorSet)
	{
		++ImplementationsCount;
		CallMode = ActorSet;
	}

	if (bImplementsProvideLocationsSet)
	{
		++ImplementationsCount;
		CallMode = LocationSet;
	}

	if (ImplementationsCount != 1)
	{

	}
}

void UEnvQueryContext_BlueprintBase::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	AActor* QuerierActor = Cast<AActor>(QueryInstance.Owner.Get());

	if (QuerierActor == NULL || CallMode == InvalidCallMode)
	{
		return;
	}
	
	switch (CallMode)
	{
	case SingleActor:
		{
			AActor* ResultingActor = NULL;
			ProvideSingleActor(QuerierActor, ResultingActor);
			UEnvQueryItemType_Actor::SetContextHelper(ContextData, ResultingActor);
		}
		break;
	case SingleLocation:
		{
			FVector ResultingLocation = FAISystem::InvalidLocation;
			ProvideSingleLocation(QuerierActor, ResultingLocation);
			UEnvQueryItemType_Point::SetContextHelper(ContextData, ResultingLocation);
		}
		break;
	case ActorSet:
		{
			TArray<AActor*> ActorSet;
			ProvideActorsSet(QuerierActor, ActorSet);
			UEnvQueryItemType_Actor::SetContextHelper(ContextData, ActorSet);
		}
		break;
	case LocationSet:
		{
			TArray<FVector> LocationSet;
			ProvideLocationsSet(QuerierActor, LocationSet);
			UEnvQueryItemType_Point::SetContextHelper(ContextData, LocationSet);
		}
		break;
	default:
		break;
	}
}

UWorld* UEnvQueryContext_BlueprintBase::GetWorld() const
{
	check(GetOuter() != NULL);
	
	UEnvQueryManager* EnvQueryManager = Cast<UEnvQueryManager>(GetOuter());
	if (EnvQueryManager != NULL)
	{
		return EnvQueryManager->GetWorld();
	}

	// Outer should always be UEnvQueryManager* in the game, which implements GetWorld() and therefore makes this
	// a correct world context.  However, in the editor the outer is /Script/AIModule (at compile time), which
	// does not explicitly implement GetWorld().  For that reason, calling GetWorld() generically in that case on the
	// AIModule calls to the base implementation, which causes a blueprint compile warning in the Editor
	// which states that the function isn't safe to call on this (due to requiring WorldContext which it doesn't
	// provide).  Simply returning NULL in this case fixes those erroneous blueprint compile warnings.
	return NULL;
}