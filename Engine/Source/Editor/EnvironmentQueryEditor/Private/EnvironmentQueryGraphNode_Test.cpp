// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "EnvironmentQuery/EnvQueryTest.h"

UEnvironmentQueryGraphNode_Test::UEnvironmentQueryGraphNode_Test(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bTestEnabled = true;
	bHasNamedWeight = false;
	TestWeightPct = -1.0f;
}

void UEnvironmentQueryGraphNode_Test::PostPlacedNewNode()
{
	if (EnvQueryNodeClass != NULL)
	{
		UEnvQuery* Query = Cast<UEnvQuery>(GetEnvironmentQueryGraph()->GetOuter());
		NodeInstance = ConstructObject<UEnvQueryTest>(EnvQueryNodeClass, Query);
		NodeInstance->SetFlags(RF_Transactional);

		UEnvQueryTest* TestInstance = Cast<UEnvQueryTest>(NodeInstance);
		TestInstance->UpdateTestVersion();
	}

	if (ParentNode)
	{
		ParentNode->CalculateWeights();
	}
}

void UEnvironmentQueryGraphNode_Test::DestroyNode()
{
	if (ParentNode)
	{
		ParentNode->Modify();
		ParentNode->Tests.Remove(this);
	}

	Super::DestroyNode();
}

FText UEnvironmentQueryGraphNode_Test::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UEnvQueryTest* TestInstance = Cast<UEnvQueryTest>(NodeInstance);
	return TestInstance ? FText::FromString(TestInstance->GetDescriptionTitle()) : FText::GetEmpty();
}

FText UEnvironmentQueryGraphNode_Test::GetDescription() const
{
	UEnvQueryTest* TestInstance = Cast<UEnvQueryTest>(NodeInstance);
	return TestInstance ? TestInstance->GetDescriptionDetails() : FText::GetEmpty();
}

void UEnvironmentQueryGraphNode_Test::SetDisplayedWeight(float Pct, bool bNamed)
{
	if (TestWeightPct != Pct || bHasNamedWeight != bNamed)
	{
		Modify();
	}

	TestWeightPct = Pct;
	bHasNamedWeight = bNamed;
}
