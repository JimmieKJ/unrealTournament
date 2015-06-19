// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Random.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"

UEnvQueryTest_Random::UEnvQueryTest_Random(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;

	// "Random" is completely agnostic on item type!  Any type at all is valid!
	ValidItemType = UEnvQueryItemType::StaticClass();
}

void UEnvQueryTest_Random::RunTest(FEnvQueryInstance& QueryInstance) const
{
	FloatValueMin.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
	float MinThresholdValue = FloatValueMin.GetValue();

	FloatValueMax.BindData(QueryInstance.Owner.Get(), QueryInstance.QueryID);
	float MaxThresholdValue = FloatValueMax.GetValue();

	// loop through all items
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		It.SetScore(TestPurpose, FilterType, FMath::FRand(), MinThresholdValue, MaxThresholdValue);
	}
}

FText UEnvQueryTest_Random::GetDescriptionDetails() const
{
	return DescribeFloatTestParams();
}
