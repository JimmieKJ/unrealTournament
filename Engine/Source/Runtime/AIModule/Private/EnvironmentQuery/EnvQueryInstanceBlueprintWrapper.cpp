// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"

UEnvQueryInstanceBlueprintWrapper::UEnvQueryInstanceBlueprintWrapper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, QueryID(INDEX_NONE)
{

}

void UEnvQueryInstanceBlueprintWrapper::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	check(Result.IsValid());
	QueryResult = Result;
	ItemType = Result->ItemType;
	OptionIndex = Result->OptionIndex;

	OnQueryFinishedEvent.Broadcast(this, Result->GetRawStatus());
}

float UEnvQueryInstanceBlueprintWrapper::GetItemScore(int32 ItemIndex)
{
	return QueryResult.IsValid() ? QueryResult->GetItemScore(ItemIndex) : -1.f;
}

TArray<AActor*> UEnvQueryInstanceBlueprintWrapper::GetResultsAsActors()
{
	TArray<AActor*> Results;

	if (QueryResult.IsValid() && ItemType->IsChildOf(UEnvQueryItemType_ActorBase::StaticClass()))
	{
		if (RunMode != EEnvQueryRunMode::AllMatching)
		{
			Results.Add(QueryResult->GetItemAsActor(0));
		}
		else
		{
			QueryResult->GetAllAsActors(Results);
		}
	}

	return Results;
}

TArray<FVector> UEnvQueryInstanceBlueprintWrapper::GetResultsAsLocations()
{
	TArray<FVector> Results;

	if (QueryResult.IsValid())
	{
		if (RunMode != EEnvQueryRunMode::AllMatching)
		{
			Results.Add(QueryResult->GetItemAsLocation(0));
		}
		else
		{
			QueryResult->GetAllAsLocations(Results);
		}
	}

	return Results;
}