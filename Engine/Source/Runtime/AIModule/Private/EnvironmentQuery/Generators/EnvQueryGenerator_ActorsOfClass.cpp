// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ActorsOfClass.h"
#include "AI/Navigation/RecastNavMesh.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_ActorsOfClass::UEnvQueryGenerator_ActorsOfClass(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SearchCenter = UEnvQueryContext_Querier::StaticClass();
	ItemType = UEnvQueryItemType_Actor::StaticClass();
	SearchRadius.DefaultValue = 500.0f;
	SearchedActorClass = AActor::StaticClass();
}

void UEnvQueryGenerator_ActorsOfClass::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	SearchRadius.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
	float RadiusValue = SearchRadius.GetValue();

	UWorld* World = GEngine->GetWorldFromContextObject(QueryInstance.Owner.Get());
	if (World == NULL || SearchedActorClass == NULL)
	{
		return;
	}

	const float RadiusSq = FMath::Square(RadiusValue);

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(SearchCenter, ContextLocations);

	for (TActorIterator<AActor> ItActor = TActorIterator<AActor>(World, SearchedActorClass); ItActor; ++ItActor)
	{
		for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ++ContextIndex)
		{
			if (FVector::DistSquared(ContextLocations[ContextIndex], ItActor->GetActorLocation()) < RadiusSq)
			{
				QueryInstance.AddItemData<UEnvQueryItemType_Actor>(*ItActor);
				break;
			}
		}
	}
}

FText UEnvQueryGenerator_ActorsOfClass::GetDescriptionTitle() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("DescriptionTitle"), Super::GetDescriptionTitle());
	Args.Add(TEXT("ActorsClass"), FText::FromString(GetNameSafe(SearchedActorClass)));
	Args.Add(TEXT("DescribeContext"), UEnvQueryTypes::DescribeContext(SearchCenter));

	return FText::Format(LOCTEXT("DescriptionGenerateActorsAroundContext", "{DescriptionTitle}: generate set of actors of {ActorsClass} around {DescribeContext}"), Args);
};

FText UEnvQueryGenerator_ActorsOfClass::GetDescriptionDetails() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Radius"), FText::FromString(SearchRadius.ToString()));

	FText Desc = FText::Format(LOCTEXT("ActorsOfClassDescription", "radius: {Radius}"), Args);

	return Desc;
}

#undef LOCTEXT_NAMESPACE