// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_Composite.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_Composite::UEnvQueryGenerator_Composite(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
	bHasMatchingItemType = true;
}

void UEnvQueryGenerator_Composite::GenerateItems(FEnvQueryInstance& QueryInstance) const 
{
	if (bHasMatchingItemType)
	{
		for (int32 Idx = 0; Idx < Generators.Num(); Idx++)
		{
			if (Generators[Idx])
			{
				Generators[Idx]->GenerateItems(QueryInstance);
			}
		}
	}
}

FText UEnvQueryGenerator_Composite::GetDescriptionTitle() const
{
	FText Desc = Super::GetDescriptionTitle();
	for (int32 Idx = 0; Idx < Generators.Num(); Idx++)
	{
		if (Generators[Idx])
		{
			Desc = FText::Format(LOCTEXT("DescTitleExtention", "{0}\n  {1}"), Desc, Generators[Idx]->GetDescriptionTitle());
		}
	}

	return Desc;
};

void UEnvQueryGenerator_Composite::VerifyItemTypes()
{
	TSubclassOf<UEnvQueryItemType> CommonItemType = nullptr;
	bHasMatchingItemType = true;

	for (int32 Idx = 0; Idx < Generators.Num(); Idx++)
	{
		if (Generators[Idx])
		{
			if (CommonItemType)
			{
				if (CommonItemType != Generators[Idx]->ItemType)
				{
					bHasMatchingItemType = false;
					break;
				}
			}
			else
			{
				CommonItemType = Generators[Idx]->ItemType;
			}
		}
	}

	if (bHasMatchingItemType)
	{
		ItemType = CommonItemType;
	}
	else
	{
		ItemType = UEnvQueryItemType_Point::StaticClass();
	}
}

#undef LOCTEXT_NAMESPACE